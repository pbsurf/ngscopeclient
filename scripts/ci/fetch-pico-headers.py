#!/usr/bin/env python3
"""Download the Pico SDK headers for building the pico transport (lib/picobridge), for CI.

The headers aren't in the repo (they're Copyright Pico Technology). This takes them from the .deb packages in Pico's
apt repository, which work for every platform (the headers have the WIN32 branches), so it runs the same on Windows
and Linux runners and needs no installer. Only the headers are kept: the libraries are loaded at run time and aren't
needed to build. Don't upload the output as an artifact or cache, it stays on the runner.

Usage: fetch-pico-headers.py <output dir>
Then configure with -DPICO_SDK_INCLUDE_DIR=<output dir> (it gets one lib<api>/ subfolder per API, the layout of
/opt/picoscope/include, which lib/picobridge/CMakeLists.txt searches with PATH_SUFFIXES).
"""

import gzip
import hashlib
import io
import os
import sys
import tarfile
import subprocess

REPO = "https://labs.picotech.com/rc/picoscope7/debian/"
INDEX = REPO + "dists/picoscope/main/binary-amd64/Packages.gz"
APIS = ["ps2000a", "ps3000a", "ps4000a", "ps5000a", "ps6000a", "psospa"]
INCLUDE_PREFIX = "opt/picoscope/include/"


def fetch(url):
	# curl (on every runner, Windows 10+ has it too) rather than urllib: the server answers 403 to urllib
	return subprocess.run(["curl", "-fsSL", "--retry", "3", "--max-time", "300", url],
		check=True, stdout=subprocess.PIPE).stdout


def parse_index(data):
	"""Package name -> list of its stanzas (dicts of fields). Pico's index has one version per package."""
	packages = {}
	for stanza in gzip.decompress(data).decode("utf-8").split("\n\n"):
		fields = {}
		for line in stanza.splitlines():
			if line and not line[0].isspace() and ":" in line:
				key, value = line.split(":", 1)
				fields[key] = value.strip()
		if "Package" in fields:
			packages.setdefault(fields["Package"], []).append(fields)
	return packages


def ar_member(deb, prefix):
	"""Return (name, body) of the first member of an ar archive (.deb) whose name starts with prefix."""
	if deb[:8] != b"!<arch>\n":
		raise ValueError("not an ar archive")
	pos = 8
	while pos + 60 <= len(deb):
		name = deb[pos:pos + 16].decode("ascii").strip().rstrip("/")
		size = int(deb[pos + 48:pos + 58].decode("ascii").strip())
		body = deb[pos + 60:pos + 60 + size]
		if name.startswith(prefix):
			return name, body
		pos += 60 + size + (size & 1)
	raise ValueError("no " + prefix + " member")


def main():
	if len(sys.argv) != 2:
		print(__doc__)
		return 2
	outdir = sys.argv[1]
	packages = parse_index(fetch(INDEX))

	for api in APIS:
		stanzas = packages.get("lib" + api)
		if not stanzas:
			raise SystemExit("lib%s not in %s" % (api, INDEX))
		if len(stanzas) > 1:
			print("lib%s: %d versions in the index, using the last one" % (api, len(stanzas)))
		pkg = stanzas[-1]
		deb = fetch(REPO + pkg["Filename"])
		if hashlib.sha256(deb).hexdigest() != pkg["SHA256"].lower():
			raise SystemExit("SHA-256 mismatch for " + pkg["Filename"])

		_, data = ar_member(deb, "data.tar")
		count = 0
		with tarfile.open(fileobj=io.BytesIO(data)) as tar:
			for m in tar.getmembers():
				name = m.name[2:] if m.name.startswith("./") else m.name
				if not (m.isfile() and name.startswith(INCLUDE_PREFIX) and name.endswith(".h")):
					continue
				rel = name[len(INCLUDE_PREFIX):]
				if ".." in rel.split("/"):
					continue
				dest = os.path.join(outdir, *rel.split("/"))
				os.makedirs(os.path.dirname(dest), exist_ok=True)
				with tar.extractfile(m) as src, open(dest, "wb") as dst:
					dst.write(src.read())
				count += 1
		if count == 0:
			raise SystemExit("no headers in " + pkg["Filename"])
		print("lib%s %s: %d headers" % (api, pkg["Version"], count))
	return 0


if __name__ == "__main__":
	sys.exit(main())
