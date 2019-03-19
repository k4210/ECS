#pragma once

#include <chrono>
#include <atomic>
#include <vector>
#include <assert.h>
#include "ECSBase.h"

#ifndef NDEBUG // DEBUG
#define ECS_STAT_ENABLED 1
#define ECS_LOG_ENABLED 1
#else // RELEASE
#define ECS_STAT_ENABLED 1
#define ECS_LOG_ENABLED  0
#endif

#undef STAT
namespace ECS
{
	enum EPredefinedStatGroups
	{
		InnerLibrary = 0,
		Framework,
		ExecutionNode,
		Custom,
		_Count
	};

	namespace Details
	{
		enum class EStatId
		{
			FindTaskToExecute,
			PushEvent,
			PopEvent,
			_Count
		};
	}
#if ECS_STAT_ENABLED

	struct Stat
	{
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

		using FStatToStr = std::add_pointer<const char*(uint32_t)>::type;

		struct RecordGroup
		{
			std::vector<Record> records;
			FStatToStr stat_to_str = nullptr;
		};

		struct StaticData
		{
			std::vector<RecordGroup> groups;

		private:
			StaticData() : groups(4)
			{
				AddGroup(Details::EStatId::_Count, EPredefinedStatGroups::InnerLibrary, [](uint32_t idx) -> const char*
				{
					const Details::EStatId id = static_cast<Details::EStatId>(idx);
					switch (id)
					{
						case Details::EStatId::FindTaskToExecute: return "FindTaskToExecute";
						case Details::EStatId::PushEvent: return "PushEvent";
						case Details::EStatId::PopEvent: return "PopEvent";
					}
					return "unknown";
				});
			}
		public:
			static StaticData& Get()
			{
				static StaticData local_inst;
				return local_inst;
			}

			template<typename RecordIdx, typename GroupIdx>
			void AddGroup(RecordIdx in_record_num, GroupIdx in_group_idx, const FStatToStr stat_to_str)
			{
				const uint32_t group_idx = static_cast<uint32_t>(in_group_idx);
				if (groups.size() <= group_idx)
				{
					groups.resize(group_idx+1);
				}
				RecordGroup& group = groups[group_idx];
				assert(group.records.empty());
				const uint32_t record_num = static_cast<uint32_t>(in_record_num);
				assert(record_num > 0);
				group.records = std::vector<Record>(record_num);
				assert(!group.stat_to_str);
				group.stat_to_str = stat_to_str;
				assert(group.stat_to_str);
			}
		};

		static void Add(const uint32_t record_index, const uint32_t group_idx, std::chrono::microseconds duration_time)
		{
			const auto microseconds = duration_time.count();
			Record& record = StaticData::Get().groups[group_idx].records[record_index];
			record.calls++;
			record.sum += microseconds;
			if (microseconds > record.max)
				record.max = microseconds;
		}

		static void Reset()
		{
			for (auto& group : StaticData::Get().groups)
			{
				for (Record& r : group.records)
				{
					r = Record{};
				}
			}
		}

		static void LogAll(int64_t frames)
		{
			printf_s("Frame: %lli\n", frames);
			for (const auto& group : StaticData::Get().groups)
			{
				for (int i = 0; i < group.records.size(); i++)
				{
					const Record& record = group.records[i];
					if (record.calls > 0)
					{
						constexpr double to_ms = 1.0 / 1000.0;
						const char* name = group.stat_to_str ? group.stat_to_str(i) : nullptr;
						printf_s("Stat %-28s avg per call: %7.3f avg per frame: %7.3f max: %7.3f calls per frame: %7.3f\n"
							, (name ? name : "unknown")
							, record.sum * to_ms / record.calls
							, record.sum * to_ms / frames
							, record.max * to_ms
							, double(record.calls) / frames);
					}
				}
			}
		}

		struct Register
		{
			template<typename RecordIdx, typename GroupIdx>
			Register(RecordIdx record_num, GroupIdx grup_idx, const FStatToStr stat_to_str)
			{
				StaticData::Get().AddGroup(record_num, grup_idx, stat_to_str);
			}
		};
	};

	inline const char* Str(ExecutionNodeId id)
	{
		auto& group = Stat::StaticData::Get().groups[static_cast<int>(EPredefinedStatGroups::ExecutionNode)];
		assert(group.stat_to_str);
		return group.stat_to_str ? group.stat_to_str(id.GetIndex()) : "unknown";
	}

	struct ScopeDurationLog
	{
	private:
		const std::chrono::time_point<std::chrono::high_resolution_clock> start;
		const uint32_t group_idx;
		const uint32_t record_index;
	public:
		ScopeDurationLog(ExecutionNodeId in_id)
			: start(std::chrono::high_resolution_clock::now())
			, group_idx(static_cast<int>(EPredefinedStatGroups::ExecutionNode)), record_index(in_id.GetIndex()) {}

		template<typename RecordIdx, typename GroupIdx>
		ScopeDurationLog(RecordIdx in_record_num, GroupIdx in_group_idx)
			: start(std::chrono::high_resolution_clock::now())
			, group_idx(static_cast<int>(in_group_idx))
			, record_index(static_cast<int>(in_record_num)) {}

		~ScopeDurationLog()
		{
			const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start);
			Stat::Add(record_index, group_idx, duration_us);
		}
	};

#if ECS_LOG_ENABLED
	namespace StatsDetails
	{
		static auto GetStartTime()
		{
			static std::chrono::time_point start = std::chrono::system_clock::now();
			return start;
		}

		template<typename... Stuff>
		inline void LogStuff(const char* format, Stuff... stuff)
		{
			const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - GetStartTime());
			printf_s(format, duration_us.count() / 1000.0f, stuff...);
		}
	}
#endif

#else // ECS_STAT_ENABLED
	struct ScopeDurationLog
	{
		template<typename T> ScopeDurationLog(T) {}
		template<typename T1, typename T2> ScopeDurationLog(T1, T2) {}
	};
#endif // ECS_STAT_ENABLED
}
#undef LOG

#if ECS_LOG_ENABLED
static_assert(ECS_STAT_ENABLED, "");
#define LOG(f, ...) ECS::StatsDetails::LogStuff("%4.3f "f"\n", __VA_ARGS__ )
#else
#define LOG(f, ...) ((void)0)
#endif