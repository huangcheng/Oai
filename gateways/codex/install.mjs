#!/usr/bin/env node
/**
 * install.mjs — Auto-install Codex hooks for Qlippy
 *
 * Usage:
 *   npx @qlippy/codex
 */

import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { homedir } from 'node:os';
import { join } from 'node:path';

const hooksPath = join(homedir(), '.codex', 'hooks.json');
const sourcePath = new URL('./hooks.json', import.meta.url);

function installHooks() {
  const hooks = JSON.parse(readFileSync(sourcePath, 'utf-8'));

  let existing = { hooks: {} };
  if (existsSync(hooksPath)) {
    try {
      existing = JSON.parse(readFileSync(hooksPath, 'utf-8'));
    } catch {
      console.error('Warning: existing hooks.json is invalid, overwriting');
    }
  }

  const merged = {
    ...existing,
    hooks: {
      ...existing.hooks,
      ...hooks.hooks,
    },
  };

  writeFileSync(hooksPath, JSON.stringify(merged, null, 2) + '\n');
  console.log(`Qlippy hooks installed to ${hooksPath}`);
}

installHooks();
