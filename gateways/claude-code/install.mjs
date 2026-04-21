#!/usr/bin/env node
/**
 * install.mjs — Auto-install Claude Code hooks for Qlippy
 *
 * Usage:
 *   npx @qlippy/claude-code
 */

import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { homedir } from 'node:os';
import { join } from 'node:path';

const settingsPath = join(homedir(), '.claude', 'settings.json');
const hooksPath = new URL('./settings.json', import.meta.url);

function mergeSettings() {
  const hooks = JSON.parse(readFileSync(hooksPath, 'utf-8'));

  let existing = {};
  if (existsSync(settingsPath)) {
    try {
      existing = JSON.parse(readFileSync(settingsPath, 'utf-8'));
    } catch {
      console.error('Warning: existing settings.json is invalid, overwriting');
    }
  }

  const merged = {
    ...existing,
    command: {
      ...existing.command,
      ...hooks.command,
    },
  };

  writeFileSync(settingsPath, JSON.stringify(merged, null, 2) + '\n');
  console.log(`Qlippy hooks installed to ${settingsPath}`);
}

mergeSettings();
