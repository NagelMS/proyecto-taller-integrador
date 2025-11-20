MAIN = main

all: pdf

pdf:
	@echo "Compilando LaTeX..."
	pdflatex $(MAIN).tex
	@echo "Corriendo Biber..."
	biber $(MAIN)
	pdflatex $(MAIN).tex
	pdflatex $(MAIN).tex
	@echo "Listo, PDF generado."

clean:
	@echo "Limpiando archivos temporales..."
	rm -f *.aux *.log *.bbl *.blg *.out *.toc *.bcf *.run.xml

view: pdf
	xdg-open $(MAIN).pdf