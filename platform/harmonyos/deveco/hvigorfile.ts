import { appTasks } from '@ohos/hvigor-ohos-plugin';

// DevEco enables isolated type checking for hvigorfile.ts and does not expose
// Node.js type declarations. Declare only the runtime globals used here so the
// build remains type-safe without adding @types/node to the application.
declare const require: (moduleName: string) => any;
declare const __dirname: string;

const fs = require('fs');
const path = require('path');

type SigningMaterial = {
    storeFile: string;
    storePassword: string;
    keyAlias: string;
    keyPassword: string;
    signAlg: 'SHA256withECDSA';
    profile: string;
    certpath: string;
};

type LocalSigningConfig = {
    name: string;
    type: 'HarmonyOS' | 'OpenHarmony';
    material: SigningMaterial;
};

const localSigningPath = path.resolve(__dirname, 'signing.local.json');
let localSigning: LocalSigningConfig | undefined;

if (fs.existsSync(localSigningPath)) {
    localSigning = JSON.parse(fs.readFileSync(localSigningPath, 'utf8')) as LocalSigningConfig;
}

// Only AGC-issued HarmonyOS material can be injected into the HarmonyOS build.
// OpenHarmony local material is applied after packaging by sign_unsigned_hap.ps1.
const harmonySigning = localSigning?.type === 'HarmonyOS' ? {
    type: localSigning.type,
    material: localSigning.material
} : undefined;

export default {
    system: appTasks,
    plugins: [],
    config: harmonySigning ? {
        ohos: {
            overrides: {
                signingConfig: harmonySigning
            }
        }
    } : {}
};
