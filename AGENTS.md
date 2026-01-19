# Agent workflow rules (hw-sw-contract-fuzzer)

## Change-control before edits

Before making any code/content changes (including calling `apply_patch` or running shell commands that write/modify files), the agent must:

1. Propose **2–3 options**, each stating:
   - What will change (files/areas)
   - The approach at a high level
   - Trade-offs/risks
2. Wait for explicit user confirmation in this form:
   - `选 <N>, apply` (or `Option <N>, apply`)

Read-only inspection commands (e.g., `ls`, `rg`, `cat`, `sed -n`, `git diff`) are allowed without this confirmation.

## Token minimization

Keep responses concise to reduce token usage:

1. Use tables, bullet points instead of paragraphs
2. Omit filler words ("I will now...", "Let me explain...")
3. Code snippets: show only relevant lines, not full files
4. Answers: 1-3 sentences when possible

**Bad:** "Let me explain what's happening here. The issue is that the function is using an uppercase O instead of lowercase o, which causes the file not to be created."

**Good:** "Bug: `-O` should be `-o`. SAIL doesn't recognize uppercase."
