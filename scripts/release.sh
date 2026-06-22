#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# --- CONFIGURATION ---
# Automatically detects repo info, or you can hardcode them if needed
REPO_OWNER=$(gh repo view --json owner -q .owner.login)
REPO_NAME=$(gh repo view --json name -q .name)
ASSETS_DIR="./dist" # Change this to your build/artifacts folder

# --- FUNCTIONS ---
log() {
    echo -e "\033[1;34m==>\033[0m $1"
}

error_exit() {
    echo -e "\033[1;31mError:\033[0m $1" >&2
    exit 1
}

# --- VALIDATION ---
if [ -z "$1" ]; then
    error_exit "Usage: $0 <vX.Y.Z> [release_notes_file]"
fi

TAG=$1
NOTES_FILE=$2
NOTES_ARG=""

# Handle release notes if provided
if [ -n "$NOTES_FILE" ]; then
    if [ -f "$NOTES_FILE" ]; then
        NOTES_ARG="--notes-file $NOTES_FILE"
    else
        error_exit "Notes file '$NOTES_FILE' not found."
    fi
else
    # Automatically generate release notes from commit history if no file is given
    NOTES_ARG="--generate-notes"
fi

# --- EXECUTION ---
log "Starting release process for ${REPO_OWNER}/${REPO_NAME} @ ${TAG}..."

# 1. Ensure local tags are up to date and create the new tag
log "Tagging repository with ${TAG}..."
git tag -a "$TAG" -m "Release $TAG"
git push origin "$TAG"

# 2. Gather assets to upload (DRY approach to asset management)
ASSET_PARAMS=""
if [ -d "$ASSETS_DIR" ] && [ "$(ls -A $ASSETS_DIR)" ]; then
    log "Finding assets in ${ASSETS_DIR}..."
    for file in "$ASSETS_DIR"/*; do
        if [ -f "$file" ]; then
            ASSET_PARAMS="$ASSET_PARAMS \"$file\""
        fi
    done
else
    log "No assets found in ${ASSETS_DIR} (skipping uploads)."
fi

# 3. Create the GitHub Release
log "Creating GitHub release..."
eval "gh release create \"$TAG\" $ASSET_PARAMS $NOTES_ARG --title \"Release $TAG\""

log "Successfully released $TAG!"