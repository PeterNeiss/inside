# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Builds inside.html from inside.md with mpark/wg21, cloned on first use at a
# pinned commit (wg21/ is gitignored; it downloads its own pinned pandoc).
#
#   make -C docs/proposal          # inside.html
#   make -C docs/proposal check    # listings equal docs/proposal/examples/*.cpp

WG21_URL := https://github.com/mpark/wg21.git
WG21_REF := c3336b531109f6853b6fd0b2bc30a45191990fbc

ifeq ($(wildcard wg21/paper.mk),)
$(shell git clone -q $(WG21_URL) wg21 >&2 && git -C wg21 checkout -q $(WG21_REF) >&2)
endif

PAPER_RULE := inside:inside
include wg21/paper.mk

.PHONY: check
check:
	python3 listings.py check
