// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/MemoryUtil.h"

#include "VideoBackends/OGL/Globals.h"
#include "VideoBackends/OGL/GLUtil.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/StreamBuffer.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace OGL
{

// moved out of constructor, so m_buffer is allowed to be const
static u32 genBuffer()
{
	u32 id;
	glGenBuffers(1, &id);
	return id;
}

StreamBuffer::StreamBuffer(u32 type, size_t size)
: m_buffer(genBuffer()), m_buffertype(type), m_size(size)
{
	m_iterator = 0;
	m_used_iterator = 0;
	m_free_iterator = 0;
	fences = nullptr;
}


StreamBuffer::~StreamBuffer()
{
	glDeleteBuffers(1, &m_buffer);
}

/* Shared synchronisation code for ring buffers
 * 
 * The next three functions are to create/delete/use the OpenGL synchronisation.
 * ARB_sync (OpenGL 3.2) is used and required.
 * 
 * To reduce overhead, the complete buffer is splitted up into SYNC_POINTS chunks.
 * For each of this chunks, there is a fence which checks if this chunk is still in use.
 * 
 * As our API allows to alloc more memory then it has to use, we have to catch how much is already written.
 * 
 * m_iterator      - writing position
 * m_free_iterator - last position checked if free
 * m_used_iterator - last position known to be written
 * 
 * So on alloc, we have to wait for all slots between m_free_iterator and m_iterator (and set m_free_iterator to m_iterator afterwards).
 * 
 * We also assume that this buffer is accessed by the gpu between the Unmap and Map function,
 * so we may create the fences on the start of mapping.
 * Some here, new fences for the chunks between m_used_iterator and m_iterator (also update m_used_iterator).
 * 
 * As ring buffers have an ugly behavoir on rollover, have fun to read this code ;)
 */

#define SLOT(x) ((x)*SYNC_POINTS/m_size)
static const u32 SYNC_POINTS = 16;
void StreamBuffer::CreateFences()
{
	fences = new GLsync[SYNC_POINTS];
	for (u32 i=0; i<SYNC_POINTS; i++)
		fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}
void StreamBuffer::DeleteFences()
{
	for (size_t i = SLOT(m_free_iterator) + 1; i < SYNC_POINTS; i++)
	{
		glDeleteSync(fences[i]);
	}
	for (size_t i = 0; i < SLOT(m_iterator); i++)
	{
		glDeleteSync(fences[i]);
	}
	delete [] fences;
}
void StreamBuffer::AllocMemory(size_t size)
{
	// insert waiting slots for used memory
	for (size_t i = SLOT(m_used_iterator); i < SLOT(m_iterator); i++)
	{
		fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	}
	m_used_iterator = m_iterator;

	// wait for new slots to end of buffer
	for (size_t i = SLOT(m_free_iterator) + 1; i <= SLOT(m_iterator + size) && i < SYNC_POINTS; i++)
	{
		glClientWaitSync(fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
		glDeleteSync(fences[i]);
	}
	m_free_iterator = m_iterator + size;

	// if buffer is full
	if (m_iterator + size >= m_size) {

		// insert waiting slots in unused space at the end of the buffer
		for (size_t i = SLOT(m_used_iterator); i < SYNC_POINTS; i++)
		{
			fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}

		// move to the start
		m_used_iterator = m_iterator = 0; // offset 0 is always aligned

		// wait for space at the start
		for (u32 i = 0; i <= SLOT(m_iterator + size); i++)
		{
			glClientWaitSync(fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
			glDeleteSync(fences[i]);
		}
		m_free_iterator = m_iterator + size;
	}
}
#undef SLOT

void StreamBuffer::Align(u32 stride)
{
	if (m_iterator && stride) {
		m_iterator--;
		m_iterator = m_iterator - (m_iterator % stride) + stride;
	}
}

/* The usual way to stream data to the gpu.
 * Described here: https://www.opengl.org/wiki/Buffer_Object_Streaming#Unsynchronized_buffer_mapping
 * Just do unsync appends until the buffer is full.
 * When it's full, orphan (alloc a new buffer and free the old one)
 * 
 * As reallocation is an overhead, this method isn't as fast as it is known to be.
 */
class MapAndOrphan : public StreamBuffer
{
public:
	MapAndOrphan(u32 type, size_t size) : StreamBuffer(type, size) {
		glBindBuffer(m_buffertype, m_buffer);
		glBufferData(m_buffertype, m_size, nullptr, GL_STREAM_DRAW);
	}

	~MapAndOrphan() {
	}

	std::pair<u8*, size_t> Map(size_t size, u32 stride) override {
		Align(stride);
		if (m_iterator + size >= m_size) {
			glBufferData(m_buffertype, m_size, nullptr, GL_STREAM_DRAW);
			m_iterator = 0;
		}
		u8* pointer = (u8*)glMapBufferRange(m_buffertype, m_iterator, size,
			GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
		return std::make_pair(pointer, m_iterator);
	}

	void Unmap(size_t used_size) override {
		glFlushMappedBufferRange(m_buffertype, 0, used_size);
		glUnmapBuffer(m_buffertype);
		m_iterator += used_size;
	}
};

/* A modified streaming way without reallocation
 * This one fixes the reallocation overhead of the MapAndOrphan one.
 * So it alloc a ring buffer on initialization.
 * But with this limited ressource, we have to care about the cpu-gpu distance.
 * Else this fifo may overflow.
 * So we had traded orphan vs syncing.
 */
class MapAndSync : public StreamBuffer
{
public:
	MapAndSync(u32 type, size_t size) : StreamBuffer(type, size) {
		CreateFences();
		glBindBuffer(m_buffertype, m_buffer);
		glBufferData(m_buffertype, m_size, nullptr, GL_STREAM_DRAW);
	}

	~MapAndSync() {
		DeleteFences();
	}

	std::pair<u8*, size_t> Map(size_t size, u32 stride) override {
		Align(stride);
		AllocMemory(size);
		u8* pointer = (u8*)glMapBufferRange(m_buffertype, m_iterator, size,
			GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
		return std::make_pair(pointer, m_iterator);
	}

	void Unmap(size_t used_size) override {
		glFlushMappedBufferRange(m_buffertype, 0, used_size);
		glUnmapBuffer(m_buffertype);
		m_iterator += used_size;
	}
};

/* Streaming fifo without mapping ovearhead.
 * This one usually requires ARB_buffer_storage (OpenGL 4.4).
 * And is usually not available on OpenGL3 gpus.
 * 
 * ARB_buffer_storage allows us to render from a mapped buffer.
 * So we map it persistently in the initialization.
 * 
 * Unsync mapping sounds like an easy task, but it isn't for threaded drivers.
 * So every mapping on current close-source driver _will_ end in
 * at least a round trip time between two threads.
 * 
 * As persistently mapped buffer can't use orphaning, we also have to sync.
 */
class BufferStorage : public StreamBuffer
{
public:
	BufferStorage(u32 type, size_t size) : StreamBuffer(type, size) {
		CreateFences();
		glBindBuffer(m_buffertype, m_buffer);

		// PERSISTANT_BIT to make sure that the buffer can be used while mapped
		// COHERENT_BIT is set so we don't have to use a MemoryBarrier on write
		// CLIENT_STORAGE_BIT is set since we access the buffer more frequently on the client side then server side
		glBufferStorage(m_buffertype, m_size, nullptr,
			GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_CLIENT_STORAGE_BIT); 
		m_pointer = (u8*)glMapBufferRange(m_buffertype, 0, m_size,
			GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT); 
	}

	~BufferStorage() {
		DeleteFences();
		glUnmapBuffer(m_buffertype);
		glBindBuffer(m_buffertype, 0);
	}

	std::pair<u8*, size_t> Map(size_t size, u32 stride) override {
		Align(stride);
		AllocMemory(size);
		return std::make_pair(m_pointer + m_iterator, m_iterator);
	}

	void Unmap(size_t used_size) override {
		m_iterator += used_size;
	}

	u8* m_pointer;
};

/* --- AMD only ---
 * Another streaming fifo without mapping overhead.
 * As we can't orphan without mapping, we have to sync.
 * 
 * This one uses AMD_pinned_memory which is available on all AMD gpus.
 * OpenGL 4.4 drivers should use BufferStorage.
 */
class PinnedMemory : public StreamBuffer
{
public:
	PinnedMemory(u32 type, size_t size) : StreamBuffer(type, size) {
		CreateFences();
		m_pointer = (u8*)AllocateAlignedMemory(ROUND_UP(m_size,ALIGN_PINNED_MEMORY), ALIGN_PINNED_MEMORY );
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, m_buffer);
		glBufferData(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, ROUND_UP(m_size,ALIGN_PINNED_MEMORY), m_pointer, GL_STREAM_COPY);
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, 0);
		glBindBuffer(m_buffertype, m_buffer);
	}

	~PinnedMemory() {
		DeleteFences();
		glBindBuffer(m_buffertype, 0);
		glFinish(); // ogl pipeline must be flushed, else this buffer can be in use
		FreeAlignedMemory(m_pointer);
		m_pointer = nullptr;
	}

	std::pair<u8*, size_t> Map(size_t size, u32 stride) override {
		Align(stride);
		AllocMemory(size);
		return std::make_pair(m_pointer + m_iterator, m_iterator);
	}

	void Unmap(size_t used_size) override {
		m_iterator += used_size;
	}

	u8* m_pointer;
	static const u32 ALIGN_PINNED_MEMORY = 4096;
};

/* Fifo based on the glBufferSubData call.
 * As everything must be copied before glBufferSubData returns,
 * an additional memcpy in the driver will be done.
 * So this is a huge overhead, only use it if required.
 */
class BufferSubData : public StreamBuffer
{
public:
	BufferSubData(u32 type, size_t size) : StreamBuffer(type, size) {
		glBindBuffer(m_buffertype, m_buffer);
		glBufferData(m_buffertype, size, nullptr, GL_STATIC_DRAW);
		m_pointer = new u8[m_size];
	}

	~BufferSubData() {
		delete [] m_pointer;
	}

	std::pair<u8*, size_t> Map(size_t size, u32 stride) override {
		return std::make_pair(m_pointer, 0);
	}

	void Unmap(size_t used_size) override {
		glBufferSubData(m_buffertype, 0, used_size, m_pointer);
	}

	u8* m_pointer;
};

/* Fifo based on the glBufferData call.
 * Some trashy drivers stall in BufferSubData.
 * So here we use glBufferData, which realloc this buffer every time.
 * This may avoid stalls, but it is a bigger overhead than BufferSubData.
 */
class BufferData : public StreamBuffer
{
public:
	BufferData(u32 type, size_t size) : StreamBuffer(type, size) {
		glBindBuffer(m_buffertype, m_buffer);
		m_pointer = new u8[m_size];
	}

	~BufferData() {
		delete [] m_pointer;
	}

	std::pair<u8*, size_t> Map(size_t size, u32 stride) override {
		return std::make_pair(m_pointer, 0);
	}

	void Unmap(size_t used_size) override {
		glBufferData(m_buffertype, used_size, m_pointer, GL_STREAM_DRAW);
	}

	u8* m_pointer;
};

// choose best streaming library based on the supported extensions and known issues
StreamBuffer* StreamBuffer::Create(u32 type, size_t size)
{
	// without basevertex support, only streaming methods whith uploads everything to zero works fine:
	if (!g_ogl_config.bSupportsGLBaseVertex)
	{
		if (!DriverDetails::HasBug(DriverDetails::BUG_BROKENBUFFERSTREAM))
			return new BufferSubData(type, size);

		// BufferData is by far the worst way, only use it if needed
		return new BufferData(type, size);
	}

	// Prefer the syncing buffers over the orphaning one
	if (g_ogl_config.bSupportsGLSync)
	{
		// try to use buffer storage whenever possible
		if (g_ogl_config.bSupportsGLBufferStorage &&
			!(DriverDetails::HasBug(DriverDetails::BUG_BROKENBUFFERSTORAGE) && type == GL_ARRAY_BUFFER))
			return new BufferStorage(type, size);

		// pinned memory is almost as fine
		if (g_ogl_config.bSupportsGLPinnedMemory &&
			!(DriverDetails::HasBug(DriverDetails::BUG_BROKENPINNEDMEMORY) && type == GL_ELEMENT_ARRAY_BUFFER))
			return new PinnedMemory(type, size);

		// don't fall back to MapAnd* for nvidia drivers
		if (DriverDetails::HasBug(DriverDetails::BUG_BROKENUNSYNCMAPPING))
			return new BufferSubData(type, size);

		// mapping fallback
		if (g_ogl_config.bSupportsGLSync)
			return new MapAndSync(type, size);
	}

	// default fallback, should work everywhere, but isn't the best way to do this job
	return new MapAndOrphan(type, size);
}

}
