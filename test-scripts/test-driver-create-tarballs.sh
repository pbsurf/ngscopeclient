#!/bin/bash
# Install dependencies
sudo apt -y update
sudo apt -y full-upgrade
sudo apt -y install git gettext-base

./scripts/create-tarballs.sh

# Copy the package to the output path
mkdir ~/artifacts
mv tarballs/* ~/artifacts