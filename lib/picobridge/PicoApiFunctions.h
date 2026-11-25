/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal                                                                                                          *
*                                                                                                                      *
* Copyright (c) 2012-2026 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author ngscopeclient contributors
	@brief List of the Pico SDK functions used by the Pico bridge, for the run time loaded function table

	Each line is PICO_FN(api, name), where api is the PicoApi::Family the function belongs to. Include this with
	PICO_FN defined as needed. Add a line here when the bridge starts using another SDK function.

	@ingroup picobridge
 */

//ps2000a
PICO_FN(PS2000A, ps2000aCloseUnit)
PICO_FN(PS2000A, ps2000aEnumerateUnits)
PICO_FN(PS2000A, ps2000aGetAnalogueOffset)
PICO_FN(PS2000A, ps2000aGetTimebase2)
PICO_FN(PS2000A, ps2000aGetUnitInfo)
PICO_FN(PS2000A, ps2000aGetValues)
PICO_FN(PS2000A, ps2000aIsReady)
PICO_FN(PS2000A, ps2000aMaximumValue)
PICO_FN(PS2000A, ps2000aOpenUnit)
PICO_FN(PS2000A, ps2000aRunBlock)
PICO_FN(PS2000A, ps2000aSetChannel)
PICO_FN(PS2000A, ps2000aSetDataBuffer)
PICO_FN(PS2000A, ps2000aSetDigitalPort)
PICO_FN(PS2000A, ps2000aSetSigGenArbitrary)
PICO_FN(PS2000A, ps2000aSetSigGenBuiltInV2)
PICO_FN(PS2000A, ps2000aSetSigGenPropertiesBuiltIn)
PICO_FN(PS2000A, ps2000aSetSimpleTrigger)
PICO_FN(PS2000A, ps2000aSetTriggerChannelConditions)
PICO_FN(PS2000A, ps2000aSetTriggerDigitalPortProperties)
PICO_FN(PS2000A, ps2000aSigGenFrequencyToPhase)
PICO_FN(PS2000A, ps2000aStop)

//ps3000a
PICO_FN(PS3000A, ps3000aChangePowerSource)
PICO_FN(PS3000A, ps3000aCloseUnit)
PICO_FN(PS3000A, ps3000aEnumerateUnits)
PICO_FN(PS3000A, ps3000aGetAnalogueOffset)
PICO_FN(PS3000A, ps3000aGetTimebase2)
PICO_FN(PS3000A, ps3000aGetUnitInfo)
PICO_FN(PS3000A, ps3000aGetValues)
PICO_FN(PS3000A, ps3000aIsReady)
PICO_FN(PS3000A, ps3000aMaximumValue)
PICO_FN(PS3000A, ps3000aOpenUnit)
PICO_FN(PS3000A, ps3000aRunBlock)
PICO_FN(PS3000A, ps3000aSetBandwidthFilter)
PICO_FN(PS3000A, ps3000aSetChannel)
PICO_FN(PS3000A, ps3000aSetDataBuffer)
PICO_FN(PS3000A, ps3000aSetDigitalPort)
PICO_FN(PS3000A, ps3000aSetSigGenArbitrary)
PICO_FN(PS3000A, ps3000aSetSigGenBuiltInV2)
PICO_FN(PS3000A, ps3000aSetSigGenPropertiesBuiltIn)
PICO_FN(PS3000A, ps3000aSetSimpleTrigger)
PICO_FN(PS3000A, ps3000aSetTriggerChannelConditionsV2)
PICO_FN(PS3000A, ps3000aSetTriggerDigitalPortProperties)
PICO_FN(PS3000A, ps3000aSigGenFrequencyToPhase)
PICO_FN(PS3000A, ps3000aStop)

//ps4000a
PICO_FN(PS4000A, ps4000aChangePowerSource)
PICO_FN(PS4000A, ps4000aCloseUnit)
PICO_FN(PS4000A, ps4000aEnumerateUnits)
PICO_FN(PS4000A, ps4000aGetAnalogueOffset)
PICO_FN(PS4000A, ps4000aGetTimebase2)
PICO_FN(PS4000A, ps4000aGetUnitInfo)
PICO_FN(PS4000A, ps4000aGetValues)
PICO_FN(PS4000A, ps4000aIsReady)
PICO_FN(PS4000A, ps4000aMaximumValue)
PICO_FN(PS4000A, ps4000aOpenUnit)
PICO_FN(PS4000A, ps4000aRunBlock)
PICO_FN(PS4000A, ps4000aSetBandwidthFilter)
PICO_FN(PS4000A, ps4000aSetChannel)
PICO_FN(PS4000A, ps4000aSetDataBuffer)
PICO_FN(PS4000A, ps4000aSetDeviceResolution)
PICO_FN(PS4000A, ps4000aSetSigGenArbitrary)
PICO_FN(PS4000A, ps4000aSetSigGenBuiltInV2)
PICO_FN(PS4000A, ps4000aSetSigGenPropertiesBuiltIn)
PICO_FN(PS4000A, ps4000aSetSimpleTrigger)
PICO_FN(PS4000A, ps4000aSigGenFrequencyToPhase)
PICO_FN(PS4000A, ps4000aStop)

//ps5000a
PICO_FN(PS5000A, ps5000aChangePowerSource)
PICO_FN(PS5000A, ps5000aCloseUnit)
PICO_FN(PS5000A, ps5000aEnumerateUnits)
PICO_FN(PS5000A, ps5000aGetAnalogueOffset)
PICO_FN(PS5000A, ps5000aGetTimebase2)
PICO_FN(PS5000A, ps5000aGetUnitInfo)
PICO_FN(PS5000A, ps5000aGetValues)
PICO_FN(PS5000A, ps5000aIsReady)
PICO_FN(PS5000A, ps5000aMaximumValue)
PICO_FN(PS5000A, ps5000aOpenUnit)
PICO_FN(PS5000A, ps5000aRunBlock)
PICO_FN(PS5000A, ps5000aSetBandwidthFilter)
PICO_FN(PS5000A, ps5000aSetChannel)
PICO_FN(PS5000A, ps5000aSetDataBuffer)
PICO_FN(PS5000A, ps5000aSetDeviceResolution)
PICO_FN(PS5000A, ps5000aSetDigitalPort)
PICO_FN(PS5000A, ps5000aSetSigGenArbitrary)
PICO_FN(PS5000A, ps5000aSetSigGenBuiltInV2)
PICO_FN(PS5000A, ps5000aSetSigGenPropertiesBuiltIn)
PICO_FN(PS5000A, ps5000aSetSimpleTrigger)
PICO_FN(PS5000A, ps5000aSetTriggerChannelConditionsV2)
PICO_FN(PS5000A, ps5000aSetTriggerDigitalPortProperties)
PICO_FN(PS5000A, ps5000aSigGenFrequencyToPhase)
PICO_FN(PS5000A, ps5000aStop)

//ps6000a
PICO_FN(PS6000A, ps6000aCloseUnit)
PICO_FN(PS6000A, ps6000aEnumerateUnits)
PICO_FN(PS6000A, ps6000aGetAdcLimits)
PICO_FN(PS6000A, ps6000aGetAnalogueOffsetLimits)
PICO_FN(PS6000A, ps6000aGetDeviceResolution)
PICO_FN(PS6000A, ps6000aGetTimebase)
PICO_FN(PS6000A, ps6000aGetUnitInfo)
PICO_FN(PS6000A, ps6000aGetValues)
PICO_FN(PS6000A, ps6000aIsReady)
PICO_FN(PS6000A, ps6000aOpenUnit)
PICO_FN(PS6000A, ps6000aRunBlock)
PICO_FN(PS6000A, ps6000aSetChannelOff)
PICO_FN(PS6000A, ps6000aSetChannelOn)
PICO_FN(PS6000A, ps6000aSetDataBuffer)
PICO_FN(PS6000A, ps6000aSetDeviceResolution)
PICO_FN(PS6000A, ps6000aSetDigitalPortOff)
PICO_FN(PS6000A, ps6000aSetDigitalPortOn)
PICO_FN(PS6000A, ps6000aSetSimpleTrigger)
PICO_FN(PS6000A, ps6000aSetTriggerChannelConditions)
PICO_FN(PS6000A, ps6000aSetTriggerChannelDirections)
PICO_FN(PS6000A, ps6000aSetTriggerChannelProperties)
PICO_FN(PS6000A, ps6000aSetTriggerDigitalPortProperties)
PICO_FN(PS6000A, ps6000aSigGenApply)
PICO_FN(PS6000A, ps6000aSigGenFrequency)
PICO_FN(PS6000A, ps6000aSigGenRange)
PICO_FN(PS6000A, ps6000aSigGenWaveform)
PICO_FN(PS6000A, ps6000aSigGenWaveformDutyCycle)
PICO_FN(PS6000A, ps6000aStop)

//psospa
PICO_FN(PSOSPA, psospaCloseUnit)
PICO_FN(PSOSPA, psospaEnumerateUnits)
PICO_FN(PSOSPA, psospaGetAdcLimits)
PICO_FN(PSOSPA, psospaGetAnalogueOffsetLimits)
PICO_FN(PSOSPA, psospaGetDeviceResolution)
PICO_FN(PSOSPA, psospaGetTimebase)
PICO_FN(PSOSPA, psospaGetUnitInfo)
PICO_FN(PSOSPA, psospaGetValues)
PICO_FN(PSOSPA, psospaIsReady)
PICO_FN(PSOSPA, psospaOpenUnit)
PICO_FN(PSOSPA, psospaRunBlock)
PICO_FN(PSOSPA, psospaSetChannelOff)
PICO_FN(PSOSPA, psospaSetChannelOn)
PICO_FN(PSOSPA, psospaSetDataBuffer)
PICO_FN(PSOSPA, psospaSetDeviceResolution)
PICO_FN(PSOSPA, psospaSetDigitalPortOff)
PICO_FN(PSOSPA, psospaSetDigitalPortOn)
PICO_FN(PSOSPA, psospaSetSimpleTrigger)
PICO_FN(PSOSPA, psospaSetTriggerChannelConditions)
PICO_FN(PSOSPA, psospaSetTriggerChannelProperties)
PICO_FN(PSOSPA, psospaSetTriggerDigitalPortProperties)
PICO_FN(PSOSPA, psospaSigGenApply)
PICO_FN(PSOSPA, psospaSigGenFrequency)
PICO_FN(PSOSPA, psospaSigGenRange)
PICO_FN(PSOSPA, psospaSigGenWaveform)
PICO_FN(PSOSPA, psospaSigGenWaveformDutyCycle)
PICO_FN(PSOSPA, psospaStop)

