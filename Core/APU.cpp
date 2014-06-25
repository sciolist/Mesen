#pragma once

#include "stdafx.h"
#include "APU.h"
#include "CPU.h"

APU* APU::Instance = nullptr;
IAudioDevice* APU::AudioDevice = nullptr;

APU::APU(MemoryManager* memoryManager)
{
	APU::Instance = this;

	_memoryManager = memoryManager;

	_buf.sample_rate(APU::SampleRate);
	_buf.clock_rate(CPU::ClockRate);
	_apu.output(&_buf);

	_apu.dmc_reader(&APU::DMCRead);
	//_apu.irq_notifier(&APU::IRQChanged);

	_outputBuffer = new int16_t[APU::SamplesPerFrame];
}

APU::~APU()
{
	delete[] _outputBuffer;
}

void APU::Reset()
{
	_apu.reset();
}

int APU::DMCRead(void*, cpu_addr_t addr)
{
	return APU::Instance->_memoryManager->Read(addr);
}

uint8_t APU::ReadRAM(uint16_t addr)
{
	switch(addr) {
		case 0x4015:
			CPU::ClearIRQSource(IRQSource::FrameCounter);
			return _apu.read_status(5);
	}

	return 0;
}

void APU::WriteRAM(uint16_t addr, uint8_t value)
{
	_apu.write_register(5, addr, value);
}

bool APU::Exec(uint32_t executedCycles)
{
	_currentClock += executedCycles;

	if(_currentClock >= 29780) {
		_apu.end_frame(_currentClock);
		_buf.end_frame(_currentClock);

		_currentClock -= 29780;

		if(APU::Instance->_apu.earliest_irq() == Nes_Apu::irq_waiting) {
			CPU::SetIRQSource(IRQSource::FrameCounter);
		}

		// Read some samples out of Blip_Buffer if there are enough to fill our output buffer
		uint32_t availableSampleCount = _buf.samples_avail();
		if(availableSampleCount >= APU::SamplesPerFrame) {
			size_t sampleCount = _buf.read_samples(_outputBuffer, APU::SamplesPerFrame);
			APU::AudioDevice->PlayBuffer(_outputBuffer, sampleCount * BitsPerSample / 8);

			return true;
		}
	}
	return false;
}
