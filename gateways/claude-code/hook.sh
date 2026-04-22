#!/bin/bash
# Qlippy hook for Claude Code — sends events with session name
NODE="/Users/huangcheng/Library/Application Support/fnm/node-versions/v24.14.0/installation/bin/node"
CLI="/Users/huangcheng/Projects/Clippy/gateways/qlippy-gateway/cli.mjs"
SESSION=$(cat ~/.claude/sessions/$PPID.json 2>/dev/null | "$NODE" -e "let d='';process.stdin.on('data',c=>d+=c);process.stdin.on('end',()=>{try{console.log(JSON.parse(d).name||'')}catch{}})" 2>/dev/null)
"$NODE" "$CLI" --source claude-code --session "$SESSION" --event "$1"
