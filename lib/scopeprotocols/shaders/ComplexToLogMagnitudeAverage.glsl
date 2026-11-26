/***********************************************************************************************************************
*                                                                                                                      *
* libscopeprotocols                                                                                                    *
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

#version 430
#pragma shader_stage(compute)

layout(std430, binding=0) restrict readonly buffer buf_din
{
	float din[];
};

layout(std430, binding=1) restrict writeonly buffer buf_dout
{
	float dout[];
};

layout(std430, push_constant) uniform constants
{
	uint npoints;
	uint nsegments;
	float scale;
};

layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

/**
	@brief Averages the power of a batch of FFTs and converts it to log magnitude in dBm

	The input is nsegments FFTs of npoints complex values each, one after another (interleaved real/imaginary). Each
	output bin is the mean power of that bin across the FFTs, times scale. Used for the one sided output of a real FFT
	(npoints = FFT size / 2 + 1), in the same bin order as the input.
 */
void main()
{
	uint i = (gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x) + gl_GlobalInvocationID.x;

	//If off end of array, stop
	if(i >= npoints)
		return;

	float sum = 0;
	for(uint s=0; s<nsegments; s++)
	{
		uint base = (s*npoints + i) * 2;
		float real = din[base];
		float imag = din[base + 1];
		sum += real*real + imag*imag;
	}

	dout[i] = (10 * log(sum * scale / nsegments) / log(10)) + 30;
}
