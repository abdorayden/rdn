PDFLATEX ?= pdflatex
TEXMFVAR ?= /tmp/rdn-texmf-var
TARGET := manual

.PHONY: all clean

all: $(TARGET).pdf

$(TARGET).pdf: $(TARGET).tex
	TEXMFVAR=$(TEXMFVAR) $(PDFLATEX) -interaction=nonstopmode -halt-on-error $(TARGET).tex
	TEXMFVAR=$(TEXMFVAR) $(PDFLATEX) -interaction=nonstopmode -halt-on-error $(TARGET).tex

clean:
	rm -f $(TARGET).aux $(TARGET).log $(TARGET).out $(TARGET).toc $(TARGET).pdf
