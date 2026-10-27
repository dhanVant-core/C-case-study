.PHONY: all test report clean verify

all: test report

test:
	python tools/test_power_sampling.py

report:
	python tools/ai_reporter.py

sim:
	python tools/simulate_telemetry.py

clean:
	rm -rf *.o *.hex *.elf __pycache__ tools/__pycache__

verify:
	@echo "Checking commit count..."
	git rev-list --count HEAD
	@echo "Verifying authors..."
	git shortlog -sn --all
