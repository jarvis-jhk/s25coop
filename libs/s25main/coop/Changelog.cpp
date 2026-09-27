// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "coop/Changelog.h"
#include "RTTR_Version.h"
#include "RttrConfig.h"
#include "files.h"
#include <boost/algorithm/string/trim.hpp>
#include <boost/nowide/fstream.hpp>
#include <istream>
#include <regex>
#include <sstream>

namespace coop::changelog {

namespace {
    std::string versionOverride;

    std::vector<unsigned> versionParts(const std::string& version)
    {
        std::vector<unsigned> parts;
        std::istringstream in(version);
        std::string part;
        while(std::getline(in, part, '.'))
            parts.push_back(static_cast<unsigned>(std::stoul(part)));
        return parts;
    }

    /// Markdown a player should not see as raw characters: `code`, **bold**, [text](link) -> text
    std::string stripMarkdown(std::string text)
    {
        static const std::regex link(R"(\[([^\]]*)\]\([^)]*\))");
        text = std::regex_replace(text, link, "$1");
        std::string out;
        out.reserve(text.size());
        for(char c : text)
        {
            if(c != '`' && c != '*')
                out += c;
        }
        return out;
    }
} // namespace

bool isReleaseVersion(const std::string& version)
{
    static const std::regex release(R"(\d{1,6}(\.\d{1,6}){1,3})");
    return std::regex_match(version, release);
}

int compareVersions(const std::string& lhs, const std::string& rhs)
{
    auto a = versionParts(lhs);
    auto b = versionParts(rhs);
    const auto len = std::max(a.size(), b.size());
    a.resize(len, 0);
    b.resize(len, 0);
    for(size_t i = 0; i < len; ++i)
    {
        if(a[i] != b[i])
            return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

std::vector<Section> parse(std::istream& in)
{
    std::vector<Section> sections;
    std::string line;
    while(std::getline(in, line))
    {
        if(!line.empty() && line.back() == '\r')
            line.pop_back();
        if(line.rfind("## ", 0) == 0)
        {
            sections.push_back(Section{boost::algorithm::trim_copy(line.substr(3)), {}});
            continue;
        }
        if(sections.empty())
            continue;
        const std::string trimmed = boost::algorithm::trim_copy(line);
        if(trimmed.empty())
            continue;
        auto& entries = sections.back().entries;
        if(trimmed.rfind("- ", 0) == 0 || trimmed.rfind("* ", 0) == 0)
            entries.push_back(stripMarkdown(trimmed.substr(2)));
        else if(!entries.empty() && line.front() == ' ')
            entries.back() += " " + stripMarkdown(trimmed); // continuation of the bullet above
        else
            entries.push_back(stripMarkdown(trimmed));
    }
    return sections;
}

std::vector<Section> newSince(const std::vector<Section>& sections, const std::string& lastSeen,
                              const std::string& current)
{
    std::vector<Section> result;
    if(!isReleaseVersion(current))
        return result;
    const bool firstRun = !isReleaseVersion(lastSeen);
    if(!firstRun && compareVersions(lastSeen, current) >= 0)
        return result;
    for(const Section& section : sections)
    {
        if(!isReleaseVersion(section.version) || compareVersions(section.version, current) > 0)
            continue;
        if(!firstRun && compareVersions(section.version, lastSeen) <= 0)
            continue;
        result.push_back(section);
        if(firstRun)
            break;
    }
    return result;
}

std::string runningVersion()
{
    return versionOverride.empty() ? rttr::version::GetVersion() : versionOverride;
}

void overrideRunningVersion(std::string version)
{
    versionOverride = std::move(version);
}

std::vector<Section> loadInstalled()
{
    boost::nowide::ifstream file(RTTRCONFIG.ExpandPath(s25::folders::texte) / "CHANGELOG.md");
    if(!file)
        return {};
    return parse(file);
}

} // namespace coop::changelog
