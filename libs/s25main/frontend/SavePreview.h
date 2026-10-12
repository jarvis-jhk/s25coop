// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "libsiedler2/PixelBufferBGRA.h"
#include <boost/filesystem/path.hpp>
#include <optional>

namespace frontend {

constexpr unsigned savePreviewWidth = 160;
constexpr unsigned savePreviewHeight = 100;

/// Optional trailer, outside the serialized world. Old readers still load the same save.
/// Call only after the save writer has closed the successfully written file.
bool AppendSavePreview(const boost::filesystem::path& path, const libsiedler2::PixelBufferBGRA& pixels);
/// Reads metadata and skips the world bytes without decompressing/deserializing them.
/// Old saves and damaged/unknown trailers simply have no preview.
std::optional<libsiedler2::PixelBufferBGRA> ReadSavePreview(const boost::filesystem::path& path);

} // namespace frontend
