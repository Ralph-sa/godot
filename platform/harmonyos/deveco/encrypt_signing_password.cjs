'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const COMPONENT = Buffer.from([
    49, 243, 9, 115, 214, 175, 91, 184,
    211, 190, 177, 88, 101, 131, 192, 119
]);

function encrypt(key, plaintext) {
    const iv = crypto.randomBytes(12);
    const cipher = crypto.createCipheriv('aes-128-gcm', key, iv);
    const encrypted = Buffer.concat([cipher.update(plaintext), cipher.final()]);
    const authTag = cipher.getAuthTag();
    const header = Buffer.alloc(4);
    // Hvigor stores the trailing encrypted-data length (ciphertext + GCM tag)
    // in the header. It derives the IV length from the complete payload size.
    header.writeUInt32BE(encrypted.length + authTag.length, 0);
    return Buffer.concat([header, iv, encrypted, authTag]);
}

function decrypt(key, payload) {
    const encryptedDataLength = payload.readUInt32BE(0);
    const ivLength = payload.length - 4 - encryptedDataLength;
    const iv = payload.subarray(4, 4 + ivLength);
    const authTag = payload.subarray(payload.length - 16);
    const encrypted = payload.subarray(4 + ivLength, payload.length - 16);
    const decipher = crypto.createDecipheriv('aes-128-gcm', key, iv);
    decipher.setAuthTag(authTag);
    return Buffer.concat([decipher.update(encrypted), decipher.final()]);
}

function singleEntry(directory) {
    const entries = fs.readdirSync(directory).filter((name) => name !== '.DS_Store');
    if (entries.length !== 1) {
        throw new Error(`Expected one material file in ${directory}`);
    }
    return fs.readFileSync(path.join(directory, entries[0]));
}

function xorAll(buffers) {
    const result = Buffer.alloc(16);
    for (const value of buffers) {
        if (value.length !== 16) {
            throw new Error('Signing material components must be 16 bytes.');
        }
        for (let index = 0; index < 16; index += 1) {
            result[index] ^= value[index];
        }
    }
    return result;
}

function deriveRootKey(materialDir) {
    const fdDir = path.join(materialDir, 'fd');
    const fdEntries = fs.readdirSync(fdDir).filter((name) => name !== '.DS_Store').sort();
    if (fdEntries.length !== 3) {
        throw new Error('Signing material fd must contain three component directories.');
    }
    const components = fdEntries.map((name) => singleEntry(path.join(fdDir, name)));
    const salt = singleEntry(path.join(materialDir, 'ac'));
    const mixed = xorAll([...components, COMPONENT]);
    // Hvigor converts the XOR result to Buffer before deriving the root key.
    return crypto.pbkdf2Sync(mixed.toString(), salt, 10000, 16, 'sha256');
}

function initializeMaterial(materialDir) {
    const fdDir = path.join(materialDir, 'fd');
    const acDir = path.join(materialDir, 'ac');
    const ceDir = path.join(materialDir, 'ce');
    fs.mkdirSync(fdDir, { recursive: true });
    fs.mkdirSync(acDir, { recursive: true });
    fs.mkdirSync(ceDir, { recursive: true });

    for (let index = 0; index < 3; index += 1) {
        const componentDir = path.join(fdDir, String(index));
        fs.mkdirSync(componentDir, { recursive: true });
        fs.writeFileSync(path.join(componentDir, crypto.randomUUID().replaceAll('-', '')), crypto.randomBytes(16));
    }
    fs.writeFileSync(path.join(acDir, crypto.randomUUID().replaceAll('-', '')), crypto.randomBytes(16));

    const rootKey = deriveRootKey(materialDir);
    const workKey = crypto.randomBytes(16);
    fs.writeFileSync(path.join(ceDir, crypto.randomUUID().replaceAll('-', '')), encrypt(rootKey, workKey));
}

function hasMaterial(materialDir) {
    try {
        return fs.statSync(path.join(materialDir, 'fd')).isDirectory()
            && fs.statSync(path.join(materialDir, 'ac')).isDirectory()
            && fs.statSync(path.join(materialDir, 'ce')).isDirectory();
    } catch {
        return false;
    }
}

const signingDir = path.resolve(process.argv[2]);
const materialDir = path.join(signingDir, 'material');
if (!hasMaterial(materialDir)) {
    initializeMaterial(materialDir);
}

const rootKey = deriveRootKey(materialDir);
const workKey = decrypt(rootKey, singleEntry(path.join(materialDir, 'ce')));
const password = fs.readFileSync(0, 'utf8').replace(/\r?\n$/, '');
process.stdout.write(encrypt(workKey, Buffer.from(password, 'utf8')).toString('hex'));
