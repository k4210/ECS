#pragma once

#include <chrono>
#include <atomic>
#include <vector>
#include <assert.h>

#ifndef NDEBUG
#define ECS_STAT_ENABLED 1
#define ECS_LOG_ENABLED 0
#else
#define ECS_STAT_ENABLED 1
#define ECS_LOG_ENABLED 0
#endif

#undef STAT
#undef STAT_PARAM

#if ECS_STAT_ENABLED
#define STAT(x) x
#define STAT_PARAM(x) , x

namespace ECS
{
	using StatId = int;

	const char* Str(StatId id);

	struct Stat
	{
		static const constexpr StatId FindTaskToExecuteId = -1;

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

		using TRecords = std::vector<Record>;
		static TRecords records;

		using FStatToStr = std::add_pointer<const char*(StatId)>::type;
		static const FStatToStr stat_to_str;

		static void Add(StatId record_index, std::chrono::microseconds duration_time)
		{
			const auto microseconds = duration_time.count();
			const int64_t num_of_inner_stats = 1;
			const int64_t actual_idx = static_cast<int64_t>(record_index) + num_of_inner_stats;
			Record& record = records[actual_idx];
			record.calls++;
			record.sum += microseconds;
			if(microseconds > record.max)
				record.max = microseconds;
		}

		static void LogAll(int64_t frames)
		{
			printf_s("Frame: %lli\n", frames);
			for (int i = 0; i < records.size(); i++)
			{
				Record& record = records[i]; 
				if (record.calls > 0)
				{
					constexpr double to_ms = 1.0/1000.0;
					printf_s("Stat %2i %-28s avg per call: %7.3f avg per frame: %7.3f max: %7.3f calls per frame: %7.3f\n"
						, i-1, Str(i-1)
						, record.sum * to_ms / record.calls
						, record.sum * to_ms / frames
						, record.max * to_ms
						, double(record.calls) / frames);
				}
			}
		}
	};

	inline const char* Str(StatId id)
	{
		if (id == Stat::FindTaskToExecuteId)
		{
			return "FindTaskToExecute";
		}
		assert(Stat::stat_to_str);
		return Stat::stat_to_str(id);
	}

	struct ScopeDurationLog
	{
	private:
		std::chrono::time_point<std::chrono::system_clock> start;
		StatId id;
	public:
		ScopeDurationLog(StatId in_id)
			: start(std::chrono::system_clock::now())
			, id(in_id) {}

		template<typename T>
		ScopeDurationLog(T in_id)
			: start(std::chrono::system_clock::now())
			, id(static_cast<int>(in_id)) {}

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
#define LOG_PARAM(x) , x
#else
#define LOG(x)
#define LOG_PARAM(x)
#endif