#pragma once

#include "../render/Model.h"
#include "../render/Renderer2D.h"
#include <string>

bool saveStateJSON(const Document& doc, const std::string& path);
bool loadStateJSON(Document& doc, const std::string& path);
bool saveCanvasPNG(Renderer2D& renderer, const Document& doc, int outW, int outH, const std::string& filename);