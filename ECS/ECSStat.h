#pragma once

#include <chrono>
#include <atomic>

#ifndef NDEBUG
#define ECS_STAT_ENABLED 1
#define ECS_LOG_ENABLED 0
#else
#define ECS_STAT_ENABLED 0
#define ECS_LOG_ENABLED 0
#endif

#undef STAT
#undef STAT_PARAM

#if ECS_STAT_ENABLED
#define STAT(x) x
#define STAT_PARAM(x) , x

enum class StatId : int
{
	FindTaskToExecute,
	Graphic_WaitForUpdate,
	Graphic_RenderSync,
	Graphic_Update,
	Graphic_WaitForRenderSync,
	Display,
	GameMovement_Update,
	GameFrame,
	Count
};

const char* StatIdToStr(StatId id);

namespace ECS
{
	struct Stat
	{
		constexpr const static int kStatsNum = static_cast<int>(StatId::Count);

		struct Record
		{
			std::atomic_int64_t sum = 0;
			std::atomic_int64_t max = 0;
			std::atomic_int64_t calls = 0;

			Record() = default;
			Record(const Record& other)
				: sum(other.sum.load())
				, max(other.max.load())
				, calls(other.calls.load())
			{}

			Record& operator=(const Record& other)
			{
				sum = other.sum.load();
				max = other.max.load();
				calls = other.calls.load();
				return *this;
			}
		};

		using TRecords = std::array<Record, kStatsNum>;
		static TRecords records;
		static void Add(StatId record_index, std::chrono::microseconds duration_time)
		{
			const auto microseconds = duration_time.count();
			Record& record = records[static_cast<int>(record_index)];
			record.calls++;
			record.sum += microseconds;
			if(microseconds > record.max)
				record.max = microseconds;
		}

		static void Reset()
		{
			Record r;
			records.fill(r);
		}

		static void LogAll(int64_t frames)
		{
			printf_s("Frame: %lli\n", frames);
			for (int i = 0; i < kStatsNum; i++)
			{
				Record& record = records[i]; 
				if (record.calls > 0)
				{
					constexpr float to_ms = 1.0f/1000.0f;
					printf_s("Stat %i\t %s\t avg per call: %f\t avg per frame: %f\t max: %f\t calls per frame: %f\n"
						, i, StatIdToStr(static_cast<StatId>(i))
						, record.sum * to_ms / record.calls, record.sum * to_ms / frames, record.max * to_ms
						, double(record.calls) / frames);
				}
			}
		}
	};

	struct ScopeDurationLog
	{
	private:
		std::chrono::time_point<std::chrono::system_clock> start;
		StatId id;
	public:
		ScopeDurationLog(StatId in_id)
			: start(std::chrono::system_clock::now())
			, id(in_id) {}
		~ScopeDurationLog()
		{
			const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start);
			Stat::Add(id, duration_us);
		}
	};
}

#else // ECS_STAT_ENABLED
#define STAT(x)
#define STAT_PARAM(x)
#endif // ECS_STAT_ENABLED


#if ECS_LOG_ENABLED
static_assert(ECS_STAT_ENABLED, "");
#define LOG(x) x
#else
#define LOG(x)
#endif