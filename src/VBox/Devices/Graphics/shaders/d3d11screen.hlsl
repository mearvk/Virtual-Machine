/* $Id: d3d11screen.hlsl 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/*
 * Screen updates.
 *
 * fxc /nologo /Fhd3d11screen.hlsl.vs.h /Evs_screen /Tvs_5_0 d3d11screen.hlsl
 * fxc /nologo /Fhd3d11screen.hlsl.ps.h /Eps_screen /Tps_5_0 d3d11screen.hlsl
 * fxc /nologo /Fhd3d11screen.hlsl.ps_SRGB.h /Eps_screen_SRGB /Tps_5_0 d3d11screen.hlsl
 */

/*
 * Copyright (C) 2022-2025 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

Texture2D t;
sampler s;

cbuffer VSParameters
{
    float srcTexcoordScaleX;
    float srcTexcoordScaleY;
    float srcTexcoordOffsetX;
    float srcTexcoordOffsetY;
    float dstCoordScaleX;
    float dstCoordScaleY;
    float dstCoordOffsetX;
    float dstCoordOffsetY;
};

struct VSInput
{
    uint VertexID   : SV_VertexID;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float2 alpha    : TEXCOORD1;
};

VSOutput vs_screen(VSInput input)
{
    VSOutput output;

    float x = (input.VertexID & 1) ? 1.0f : -1.0f;
    float y = (input.VertexID & 2) ? -1.0f : 1.0f;
    x = x * dstCoordScaleX + dstCoordOffsetX;
    y = y * dstCoordScaleY + dstCoordOffsetY;
    output.position = float4(x, y, 0.0f, 1.0f);

    x = (input.VertexID & 1) ? 1.0f : 0.0f;
    y = (input.VertexID & 2) ? 1.0f : 0.0f;

    output.texcoord.x = x * srcTexcoordScaleX + srcTexcoordOffsetX;
    output.texcoord.y = y * srcTexcoordScaleY + srcTexcoordOffsetY;

    output.alpha = float2(1.0f, 0.0f);

    return output;
}

float4 ps_screen(VSOutput input) : SV_TARGET
{
    return float4(t.Sample(s, input.texcoord).rgb, input.alpha.x);
}

float4 ps_screen_SRGB(VSOutput input) : SV_TARGET
{
    return float4(pow(t.Sample(s, input.texcoord).rgb, 1.0f/2.2f), input.alpha.x);
}
