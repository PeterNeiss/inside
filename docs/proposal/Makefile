# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Builds each paper's HTML (inside.html, quantity.html, math.html) from its
# Markdown with mpark/wg21, cloned on first use at a pinned commit (wg21/ is
# gitignored; it downloads its own pinned pandoc).
#
#   make -C docs/proposal          # every paper
#   make -C docs/proposal math.html
#   make -C docs/proposal check    # listings equal docs/proposal/examples/*.cpp

WG21_URL := https://github.com/mpark/wg21.git
WG21_REF := c3336b531109f6853b6fd0b2bc30a45191990fbc

ifeq ($(wildcard wg21/paper.mk),)
$(shell git clone -q $(WG21_URL) wg21 >&2 && git -C wg21 checkout -q $(WG21_REF) >&2)
endif

# Without a PAPER_RULE, paper.mk builds every *.md here into ./<name>.html.
include wg21/paper.mk

.PHONY: check
check:
	python3 listings.py check
