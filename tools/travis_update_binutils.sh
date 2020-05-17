#!/bin/sh

sudo add-apt-repository ppa:debhelper/binutils --yes || true
sudo apt-get update -qq --yes || true
sudo apt-get install -qq --yes --force-yes binutils
