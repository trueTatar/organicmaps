#include "kml/type_utils.hpp"
#include "kml/types.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_utils.hpp"

#include "platform/localization.hpp"
#include "platform/preferred_languages.hpp"

#include "coding/string_utf8_multilang.hpp"

namespace kml
{
bool IsEqual(std::vector<m2::PointD> const & v1, std::vector<m2::PointD> const & v2)
{
  if (v1.size() != v2.size())
    return false;

  for (size_t i = 0; i < v1.size(); ++i)
  {
    if (!v1[i].EqualDxDy(v2[i], kMwmPointAccuracy))
      return false;
  }

  return true;
}

bool IsEqual(std::vector<geometry::PointWithAltitude> const & v1,
             std::vector<geometry::PointWithAltitude> const & v2)
{
  if (v1.size() != v2.size())
    return false;

  for (size_t i = 0; i < v1.size(); ++i)
  {
    if (!AlmostEqualAbs(v1[i], v2[i], kMwmPointAccuracy))
      return false;
  }

  return true;
}

std::string GetPreferredBookmarkStr(LocalizableString const & name, std::string const & languageNorm)
{
  if (name.size() == 1)
    return name.begin()->second;

  /// @todo Complicated logic here when transforming LocalizableString -> StringUtf8Multilang to call GetPreferredName.
  StringUtf8Multilang nameMultilang;
  for (auto const & pair : name)
    nameMultilang.AddString(pair.first, pair.second);

  auto const deviceLang = StringUtf8Multilang::GetLangIndex(languageNorm);

  std::string_view preferredName;
  if (feature::GetPreferredName(nameMultilang, deviceLang, preferredName))
    return std::string(preferredName);

  return {};
}

std::string GetPreferredBookmarkStr(LocalizableString const & name, feature::RegionData const & regionData,
                                    std::string const & languageNorm)
{
  if (name.size() == 1)
    return name.begin()->second;

  /// @todo Complicated logic here when transforming LocalizableString -> StringUtf8Multilang to call GetPreferredName.
  StringUtf8Multilang nameMultilang;
  for (auto const & pair : name)
    nameMultilang.AddString(pair.first, pair.second);

  feature::NameParamsOut out;
  feature::GetReadableName({ nameMultilang, regionData, languageNorm, false /* allowTranslit */ }, out);
  return std::string(out.primary);
}

std::string GetLocalizedFeatureType(std::vector<uint32_t> const & types)
{
  if (types.empty())
    return {};

  auto const & c = classif();
  auto const type = c.GetTypeForIndex(types.front());

  return platform::GetLocalizedTypeName(c.GetReadableObjectName(type));
}

std::string GetPreferredBookmarkName(BookmarkData const & bmData, std::string const & languageOrig)
{
  auto const languageNorm = languages::Normalize(languageOrig);
  std::string name = GetPreferredBookmarkStr(bmData.m_customName, languageNorm);
  if (name.empty())
    name = GetPreferredBookmarkStr(bmData.m_name, languageNorm);
  if (name.empty())
    name = GetLocalizedFeatureType(bmData.m_featureTypes);
  return name;
}
}  // namespace kml
