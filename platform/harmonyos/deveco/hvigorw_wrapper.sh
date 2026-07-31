#!/bin/bash
# =========================================================================
# hvigorw_wrapper.sh - CLI Build Wrapper with Module Resolution Fix  
# =========================================================================
set -euo pipefail

HVIGOR_BIN="C:/Program Files/Huawei/DevEco Studio/tools/hvigor/bin"

PRELOAD="C:/Toro/GodotHOS/.hvigor_preload.cjs"
cat > "$PRELOAD" << 'PRELOADEOF'
const path = require("path");
const fs = require("fs");
const Module = require("module");
const origResolve = Module._resolveFilename;

const HVIGOR_PKG = "C:/Program Files/Huawei/DevEco Studio/tools/hvigor/hvigor";

Module._resolveFilename = function(request, parent) {
    if (request.startsWith("@ohos/hvigor")) {
        const subPath = request.slice("@ohos/hvigor".length);
        const target = subPath ? path.join(HVIGOR_PKG, subPath) : HVIGOR_PKG;

        // Try with .js extension
        if (fs.existsSync(target + ".js")) return target + ".js";
        // Try as index directory
        if (fs.existsSync(target + "/index.js")) return target + "/index.js";
        // Try bare path
        if (fs.existsSync(target)) return target;

        // Debug
        console.error("[hvigor-fix] Failed to resolve:", request, "->", target);
    }
    return origResolve.apply(this, arguments);
};
PRELOADEOF

export DEVECO_SDK_HOME="C:/Users/happyelements/AppData/Local/OpenHarmony/Sdk/20"
export JAVA_HOME="C:/Program Files/Huawei/DevEco Studio/jbr"
export NODE_OPTIONS="-r $PRELOAD"

echo "[wrapper] Running: hvigorw $*"
echo ""
exec "$HVIGOR_BIN/hvigorw" "$@"
