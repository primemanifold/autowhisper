---
name: release
description: Create a new release of AutoWhisper to the Launchpad PPA
user-invocable: true
---

# Release AutoWhisper

Create a new release by analyzing changes and uploading to the PPA.

## Process

1. **Get current state:**
   - Run `git describe --tags --abbrev=0` to find the last tag
   - Run `git log <last-tag>..HEAD --oneline` to see commits since last release
   - Read `pyproject.toml` to get current version

2. **Analyze changes and suggest bump type:**
   - Look for keywords in commit messages:
     - "BREAKING" or "!" after type → major bump
     - "feat", "feature", "add" → minor bump
     - "fix", "patch", "docs", "chore" → patch bump
   - Present findings to user with recommended bump

3. **Confirm with user:**
   - Show: current version, suggested new version, commit summary
   - Ask user to confirm or specify different version/bump type
   - Use AskUserQuestion with options: [patch, minor, major, custom]

4. **Execute release:**
   - Run `./scripts/release.sh <bump-type>`
   - The script handles: version updates, commit, tag, build, sign, upload
   - Monitor output and report results

## Example Output

```
Analyzing changes since v0.1.0...

Commits (5):
  - fix: resolve audio capture on PipeWire
  - feat: add model download progress bar
  - docs: update README
  - fix: handle missing config file
  - chore: clean up unused imports

Recommendation: minor bump (new feature detected)
  Current: 0.1.0
  New:     0.2.0

[Asks user to confirm]
```

## Error Handling

- If working directory is dirty, tell user to commit or stash first
- If no tags exist, suggest starting with 0.1.0
- If release script fails, show the error and suggest manual steps
