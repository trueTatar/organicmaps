#include "generator/maxspeeds_collector.hpp"

#include "generator/final_processor_utils.hpp"
#include "generator/maxspeeds_parser.hpp"

#include "routing_common/maxspeed_conversion.hpp"

#include "platform/platform.hpp"

#include "coding/internal/file_data.hpp"

#include "base/assert.hpp"
#include "base/geo_object_id.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <iterator>
#include <sstream>

using namespace base;
using namespace generator;
using namespace routing;
using namespace feature;
using namespace std;

namespace
{
bool ParseMaxspeedAndWriteToStream(string const & maxspeed, SpeedInUnits & speed, ostringstream & ss)
{
  if (maxspeed.empty() || !ParseMaxspeedTag(maxspeed, speed))
    return false;

  ss << UnitsToString(speed.GetUnits()) << "," << strings::to_string(speed.GetSpeed());
  return true;
}
}  // namespace

namespace generator
{
MaxspeedsCollector::MaxspeedsCollector(string const & filename)
  : CollectorInterface(filename)
{
  m_stream.exceptions(fstream::failbit | fstream::badbit);
  m_stream.open(GetTmpFilename());
}

shared_ptr<CollectorInterface> MaxspeedsCollector::Clone(
    shared_ptr<cache::IntermediateDataReaderInterface> const &) const
{
  return make_shared<MaxspeedsCollector>(GetFilename());
}

void MaxspeedsCollector::CollectFeature(FeatureBuilder const & ft, OsmElement const & p)
{
  if (!p.IsWay())
    return;

  ostringstream ss;
  ss << p.m_id << ",";

  string maxspeedForwardStr, maxspeedBackwardStr, maxspeedAdvisoryStr;
  bool isReverse = false;

  SpeedInUnits maxspeed;
  for (auto const & t : p.Tags())
  {
    if (t.m_key == "maxspeed" && ParseMaxspeedAndWriteToStream(t.m_value, maxspeed, ss))
    {
      m_stream << ss.str() << '\n';
      return;
    }

    if (t.m_key == "maxspeed:advisory")
      maxspeedAdvisoryStr = t.m_value;
    if (t.m_key == "maxspeed:forward")
      maxspeedForwardStr = t.m_value;
    else if (t.m_key == "maxspeed:backward")
      maxspeedBackwardStr = t.m_value;
    else if (t.m_key == "oneway")
      isReverse = (t.m_value == "-1");
  }

  // Note 1. isReverse == true means feature |p| has tag "oneway" with value "-1". Now (10.2018)
  // no feature with a tag oneway==-1 and a tag maxspeed:forward/backward is found. But to
  // be on the safe side this case is handled.
  // Note 2. If oneway==-1 the order of points is changed while conversion to mwm. So it's
  // necessary to swap forward and backward as well.
  if (isReverse)
    maxspeedForwardStr.swap(maxspeedBackwardStr);

  if (ParseMaxspeedAndWriteToStream(maxspeedForwardStr, maxspeed, ss))
  {
    // Note. Keeping only maxspeed:forward and maxspeed:backward if they have the same units.
    // The exception is maxspeed:forward or maxspeed:backward have a special values
    // like "none" or "walk". In that case units mean nothing an the values should
    // be processed in a special way.
    SpeedInUnits maxspeedBackward;
    if (ParseMaxspeedTag(maxspeedBackwardStr, maxspeedBackward) &&
        HaveSameUnits(maxspeed, maxspeedBackward))
    {
      ss << "," << strings::to_string(maxspeedBackward.GetSpeed());
    }
  }
  else if (ftypes::IsLinkChecker::Instance()(ft.GetTypes()))
  {
    // Assign maxspeed:advisory for highway:xxx_link types to avoid situation when
    // not defined link speed is predicted bigger than defined parent ingoing highway speed.
    // https://www.openstreetmap.org/way/5511439#map=18/45.55465/-122.67787
    if (!ParseMaxspeedAndWriteToStream(maxspeedAdvisoryStr, maxspeed, ss))
    {
      // Write indicator that it's a link and the speed should be recalculated.
      // Idea is to set the link speed equal to the ingoing highway speed (now it's "default" and can be bigger).
      // https://www.openstreetmap.org/way/842536050#map=19/45.43449/-122.56678
      ss << UnitsToString(measurement_utils::Units::Metric) << "," << strings::to_string(routing::kCommonMaxSpeedValue);
    }
  }
  else
    return;

  m_stream << ss.str() << '\n';
}

void MaxspeedsCollector::Finish()
{
  if (m_stream.is_open())
    m_stream.close();
}

void MaxspeedsCollector::Save()
{
  CHECK(!m_stream.is_open(), ("Finish() has not been called."));
  LOG(LINFO, ("Saving maxspeed tag values to", GetFilename()));
  if (Platform::IsFileExistsByFullPath(GetTmpFilename()))
    CHECK(CopyFileX(GetTmpFilename(), GetFilename()), ());
}

void MaxspeedsCollector::OrderCollectedData() { OrderTextFileByLine(GetFilename()); }

void MaxspeedsCollector::Merge(CollectorInterface const & collector)
{
  collector.MergeInto(*this);
}

void MaxspeedsCollector::MergeInto(MaxspeedsCollector & collector) const
{
  CHECK(!m_stream.is_open() || !collector.m_stream.is_open(), ("Finish() has not been called."));
  base::AppendFileToFile(GetTmpFilename(), collector.GetTmpFilename());
}
}  // namespace generator
