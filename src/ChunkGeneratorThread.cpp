#include "Globals.h"
#include "ChunkGeneratorThread.h"
#include "Generating/ChunkGenerator.h"
#include "Generating/ChunkDesc.h"




/** If the generation queue size exceeds this number, a warning will be output */
const size_t QUEUE_WARNING_LIMIT = 1000;

/** If the generation queue size exceeds this number, chunks with no clients will be skipped */
const size_t QUEUE_SKIP_LIMIT = 500;




cChunkGeneratorThreaded::cChunkGeneratorWorkerThread::cChunkGeneratorWorkerThread(cChunkGeneratorThreaded::cChunkGeneratorThreadPool * a_Pool) : Super("Chunk Generator", a_Pool)
{}







bool cChunkGeneratorThreaded::cChunkGeneratorWorkerThread::Initialize(cPluginInterface & a_PluginInterface, cChunkSink & a_ChunkSink, cIniFile & a_IniFile)
{
	m_PluginInterface = &a_PluginInterface;
	m_ChunkSink = &a_ChunkSink;

	m_Generator = cChunkGenerator::CreateFromIniFile(a_IniFile);
	if (m_Generator == nullptr)
	{
		LOGERROR("Generator could not start, aborting the server");
		return false;
	}
	return true;
}





bool cChunkGeneratorThreaded::cChunkGeneratorThreadPool::Initialize(cPluginInterface & a_PluginInterface, cChunkSink & a_ChunkSink, cIniFile & a_IniFile)
{
	bool success = true;
	std::for_each(
		m_Workers.begin(),
		m_Workers.end(),
		[&a_PluginInterface, &a_ChunkSink, &a_IniFile, &success](auto worker)
		{
			success = success && worker->Initialize(a_PluginInterface, a_ChunkSink, a_IniFile);
		}
	);
	return success;
}





void cChunkGeneratorThreaded::cChunkGeneratorThreadPool::QueueGenerateChunk(
	cChunkCoords a_Coords,
	bool a_ForceRegeneration,
	cChunkCoordCallback * a_Callback
)
{
	// Add to queue, issue a warning if too many:
	auto size = GetQueueLength();
	if (size >= QUEUE_WARNING_LIMIT)
	{
		LOGWARN("WARNING: Adding chunk %s to generation queue; Queue is too big! (%zu)", a_Coords.ToString().c_str(), size);
	}
	Submit(QueueItem(a_Coords, a_ForceRegeneration, a_Callback));
}





void cChunkGeneratorThreaded::cChunkGeneratorWorkerThread::GenerateBiomes(cChunkCoords a_Coords, cChunkDef::BiomeMap & a_BiomeMap)
{
	cCSLock Lock(m_CS);
	if (m_Generator != nullptr)
	{
		m_Generator->GenerateBiomes(a_Coords, a_BiomeMap);
	}
}





void cChunkGeneratorThreaded::cChunkGeneratorThreadPool::GenerateBiomes(cChunkCoords a_Coords, cChunkDef::BiomeMap & a_BiomeMap)
{
	m_Workers.front()->GenerateBiomes(a_Coords, a_BiomeMap);
}





int cChunkGeneratorThreaded::cChunkGeneratorWorkerThread::GetSeed() const
{
	return m_Generator->GetSeed();
}





int cChunkGeneratorThreaded::cChunkGeneratorThreadPool::GetSeed() const
{
	return m_Workers.front()->GetSeed();
}





EMCSBiome cChunkGeneratorThreaded::cChunkGeneratorWorkerThread::GetBiomeAt(int a_BlockX, int a_BlockZ)
{
	ASSERT(m_Generator != nullptr);
	return m_Generator->GetBiomeAt(a_BlockX, a_BlockZ);
}





EMCSBiome cChunkGeneratorThreaded::cChunkGeneratorThreadPool::GetBiomeAt(int a_BlockX, int a_BlockZ)
{
	return m_Workers.front()->GetBiomeAt(a_BlockX, a_BlockZ);
}





void cChunkGeneratorThreaded::cChunkGeneratorWorkerThread::Process(QueueItem & a_Task)
{
	cCSLock Lock(m_CS);

	bool SkipEnabled = (m_Queue.Size() > QUEUE_SKIP_LIMIT);
	// Skip the chunk if it's already generated and regeneration is not forced. Report as success:
	if (!a_Task.m_ForceRegeneration && m_ChunkSink->IsChunkValid(a_Task.m_Coords))
	{
		LOGD("Chunk %s already generated, skipping generation", a_Task.m_Coords.ToString().c_str());
		if (a_Task.m_Callback != nullptr)
		{
			a_Task.m_Callback->Call(a_Task.m_Coords, true);
		}
		return;
	}

	// Skip the chunk if the generator is overloaded:
	if (SkipEnabled && !m_ChunkSink->HasChunkAnyClients(a_Task.m_Coords))
	{
		LOGWARNING("Chunk generator overloaded, skipping chunk %s", a_Task.m_Coords.ToString().c_str());
		if (a_Task.m_Callback != nullptr)
		{
			a_Task.m_Callback->Call(a_Task.m_Coords, false);
		}
		return;
	}

	// Generate the chunk:
	DoGenerate(a_Task.m_Coords);
	if (a_Task.m_Callback != nullptr)
	{
		a_Task.m_Callback->Call(a_Task.m_Coords, true);
	}
}





void cChunkGeneratorThreaded::cChunkGeneratorWorkerThread::DoGenerate(cChunkCoords a_Coords)
{
	ASSERT(m_PluginInterface != nullptr);
	ASSERT(m_ChunkSink != nullptr);

	cChunkDesc ChunkDesc(a_Coords);
	m_PluginInterface->CallHookChunkGenerating(ChunkDesc);
	m_Generator->Generate(ChunkDesc);
	m_PluginInterface->CallHookChunkGenerated(ChunkDesc);

	#ifndef NDEBUG
		// Verify that the generator has produced valid data:
		ChunkDesc.VerifyHeightmap();
	#endif

	m_ChunkSink->OnChunkGenerated(ChunkDesc);
}





cChunkGeneratorThreaded::cChunkGeneratorThreadPool::cChunkGeneratorThreadPool(int numThreads)
{
	for (int i = 0; i < numThreads; i++)
	{
		m_Workers.push_back(
			std::shared_ptr<
				cChunkGeneratorThreaded::cChunkGeneratorWorkerThread>(
				new cChunkGeneratorWorkerThread(this)));
	}
}
