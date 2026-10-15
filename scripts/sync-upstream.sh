#!/bin/bash
# Merges upstream ngscopeclient into the monorepo.
#
# 1. Merges the scopehal commit that upstream scopehal-apps points to into lib/ (subtree merge).
# 2. Merges upstream scopehal-apps. Upstream's submodule pointer updates for paths that are directories here
#    (lib, lib/log, src/imgui, src/imgui-node-editor) conflict every time; they're dropped, since the content
#    arrives through step 1 or the diffs below.
# 3. If upstream moved its docs pointer, doc/upstream follows it and the manual text changes are applied to doc/.
# 4. Prints what changed upstream in imgui, imgui-node-editor and logtools, which are applied as diffs by hand.
#
# Stops at the first conflict it can't resolve, leaving it for a person to finish (then commit and rerun).
#
# Usage: scripts/sync-upstream.sh [<upstream scopehal-apps ref>]   (default upstream-apps/master)

set -euo pipefail

DIRS="lib lib/log src/imgui src/imgui-node-editor doc"

cd "$(git rev-parse --show-toplevel)"
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
	echo "error: commit or stash your changes first" >&2
	exit 1
fi

for r in upstream-apps:https://github.com/ngscopeclient/scopehal-apps upstream-scopehal:https://github.com/ngscopeclient/scopehal; do
	git remote get-url "${r%%:*}" > /dev/null 2>&1 || git remote add "${r%%:*}" "${r#*:}"
	git fetch -q --no-tags "${r%%:*}"
done
target=$(git rev-parse --verify "${1:-upstream-apps/master}^{commit}")

# Drops upstream submodule pointers that git parked as "<dir>~<branch>" because our directory is in the way.
# Fails if anything else conflicts.
resolve_pointer_conflicts()
{
	local path dir handled
	for path in $(git diff --name-only --diff-filter=U); do
		handled=
		for dir in $DIRS; do
			case "$path" in
				"$dir~"*)
					git rm -q --cached "$path"
					rmdir "$path" 2> /dev/null || rm -f "$path"
					echo "  dropped upstream pointer for $dir"
					handled=1;;
			esac
		done
		if [ -z "$handled" ]; then
			echo "error: conflict in $path needs resolving by hand; then commit and rerun" >&2
			exit 1
		fi
	done
}

# Merges <ref> with extra merge options, resolving pointer conflicts. Does nothing if already merged.
merge()
{
	local ref=$1 message=$2
	shift 2
	if git merge-base --is-ancestor "$ref" HEAD; then
		echo "$message: already up to date"
		return
	fi
	if ! git merge -q --no-edit "$@" -m "$message" "$ref"; then
		resolve_pointer_conflicts
		git commit -q --no-edit
	fi
	echo "$message: $(git rev-parse --short HEAD)"
}

old_apps=$(git merge-base HEAD "$target")

# 1. scopehal, at the commit upstream apps uses
scopehal=$(git ls-tree "$target" lib | awk '{print $3}')
merge "$scopehal" "Merge upstream scopehal $(git rev-parse --short "$scopehal") into lib/" -Xsubtree=lib

# 2. scopehal-apps
merge "$target" "Merge upstream scopehal-apps $(git rev-parse --short "$target")"

# 3. docs: git normally carries upstream's doc pointer change over to doc/upstream by rename detection
old_docs=$(git ls-tree "$old_apps" doc | awk '{print $3}')
new_docs=$(git ls-tree "$target" doc | awk '{print $3}')
if [ "$old_docs" != "$new_docs" ]; then
	git -C doc/upstream fetch -q origin
	if [ "$(git ls-tree HEAD doc/upstream | awk '{print $3}')" != "$new_docs" ]; then
		git -C doc/upstream checkout -q "$new_docs"
		git add doc/upstream
	fi
	git submodule update -q doc/upstream
	git -C doc/upstream diff --binary "$old_docs" "$new_docs" -- . ':!*.png' ':!*.pdf' | git apply -3 --index --directory=doc
	git commit -q -m "Sync manual text with upstream scopehal-docs $(git rev-parse --short "$new_docs")"
	echo "docs: doc/upstream at $(git rev-parse --short "$new_docs"), manual text synced"
fi

# 4. Forks with rebuilt history: report what needs applying
report()
{
	local name=$1 old=$2 new=$3 url=$4 dir=$5
	if [ "$old" != "$new" ]; then
		echo
		echo "$name changed upstream: ${old:0:10} -> ${new:0:10}. In a clone of $url:"
		echo "  git diff --binary $old $new | git -C $(pwd) apply -3 --index --directory=$dir"
	fi
}
report imgui "$(git ls-tree "$old_apps" src/imgui | awk '{print $3}')" "$(git ls-tree "$target" src/imgui | awk '{print $3}')" \
	https://github.com/ngscopeclient/imgui src/imgui
report imgui-node-editor "$(git ls-tree "$old_apps" src/imgui-node-editor | awk '{print $3}')" \
	"$(git ls-tree "$target" src/imgui-node-editor | awk '{print $3}')" https://github.com/ngscopeclient/imgui-node-editor \
	src/imgui-node-editor
old_scopehal=$(git ls-tree "$old_apps" lib | awk '{print $3}')
report logtools "$(git ls-tree "$old_scopehal" log | awk '{print $3}')" "$(git ls-tree "$scopehal" log | awk '{print $3}')" \
	https://github.com/ngscopeclient/ngscopeclient-logtools lib/log
