module ProceduralSky;

import math;
import environment;

public struct ProceduralSkyData {
    float density;
    int opticalDepthSamples;
    int scatteringSamples;

    float sunRadius;
    float sunGlowWidth;
    float sunGlowStrength;

    float moonRadius;
    float moonReflectance;
    float moonGlowWidth;
    float moonGlowStrength;

    float starScale;
    float starDensity;
    float starCoreWidth;
    float starGlowWidth;
    float starGlowStrength;
    float starBrightness;
    float starMinBrightness;
    float starBrightnessVariation;
    float starBrightnessFalloff;
};

static const float PLANET_RADIUS = 6371000.0;
static const float3 PLANET_CENTER = float3(0.0, -PLANET_RADIUS, 0.0);
static const float ATMOSPHERE_HEIGHT = 100000.0;
static const float RAYLEIGH_HEIGHT = ATMOSPHERE_HEIGHT * 0.08;
static const float MIE_HEIGHT = ATMOSPHERE_HEIGHT * 0.012;

static const float3 C_RAYLEIGH = float3(5.802, 13.558, 33.100) * 1e-6;
static const float3 C_MIE = float3(3.996, 3.996, 3.996) * 1e-6;
static const float3 C_OZONE = float3(0.650, 1.881, 0.085) * 1e-6;

float2 planetIntersection(float3 rayStart, float3 rayDir) {
    return sphereIntersection(rayStart, rayDir, PLANET_CENTER, PLANET_RADIUS);
}

float2 atmosphereIntersection(float3 rayStart, float3 rayDir) {
    return sphereIntersection(rayStart, rayDir, PLANET_CENTER, PLANET_RADIUS + ATMOSPHERE_HEIGHT);
}

float phaseRayleigh(float costh) {
    return 3 * (1 + costh * costh) / (16 * PI);
}

float phaseMie(float costh, float g = 0.85) {
    g = min(g, 0.9381);
    float k = 1.55 * g - 0.55 * g * g * g;
    float kcosth = k * costh;
    return (1 - k * k) / ((4 * PI) * (1 - kcosth) * (1 - kcosth));
}

float atmosphereHeight(float3 positionWS) {
    return distance(positionWS, PLANET_CENTER) - PLANET_RADIUS;
}

float densityRayleigh(float h) {
    return exp(-max(0, h / RAYLEIGH_HEIGHT));
}

float densityMie(float h) {
    return exp(-max(0, h / MIE_HEIGHT));
}

float densityOzone(float h) {
    return max(0, 1 - abs(h - 25000.0) / 15000.0);
}

float3 atmosphereDensity(float h) {
    return float3(densityRayleigh(h), densityMie(h), densityOzone(h));
}

public float3 calculateOpticalDepth(float3 rayStart, float3 rayDir, ProceduralSkyData skyData) {
    float2 intersection = atmosphereIntersection(rayStart, rayDir);
    float rayLength = intersection.y;
    float2 ground = planetIntersection(rayStart, rayDir);
    if (ground.x >= 0.0 && ground.y > 0.0) {
        rayLength = min(rayLength, ground.x);
    }

    float step = rayLength / skyData.opticalDepthSamples;
    float3 opticalDepth = 0.0;
    for (int i = 0; i < skyData.opticalDepthSamples; ++i) {
        float3 localPosition = rayStart + rayDir * (i + 0.5) * step;
        float localHeight = atmosphereHeight(localPosition);
        float3 localDensity = atmosphereDensity(localHeight);

        opticalDepth += localDensity * step;
    }

    return opticalDepth;
}

bool planetBlocks(float3 position, float3 direction) {
    float2 hit = planetIntersection(position, direction);
    return hit.x >= 0.0 && hit.y > 0.0;
}

float3 absorb(float3 opticalDepth, ProceduralSkyData skyData) {
    float3 extinction = opticalDepth.x * C_RAYLEIGH
        + opticalDepth.y * C_MIE * 1.1
        + opticalDepth.z * C_OZONE;
    return exp(-extinction * skyData.density);
}

public float3 transmittanceToLight(float3 position, float3 direction, ProceduralSkyData skyData) {
    if (planetBlocks(position, direction)) {
        return float3(0.0);
    }

    return absorb(calculateOpticalDepth(position, direction, skyData), skyData);
}

public float3 calculateScatteringAtmosphere(
    float3 rayStart,
    float3 rayDir,
    float rayLength,
    float3 lightDir,
    float3 lightColor,
    float intensity,
    ProceduralSkyData skyData,
    out float3 transmittance
) {
    // We can reduce the number of atmospheric samples required to converge by spacing them exponentially closer to the camera.
    // This breaks space view however, so let's compensate for that with an exponent that "fades" to 1 as we leave the atmosphere.
    float rayHeight = atmosphereHeight(rayStart);
    float sampleDistributionExponent = 1 + saturate(1 - rayHeight / ATMOSPHERE_HEIGHT) * skyData.opticalDepthSamples;

    float2 intersection = atmosphereIntersection(rayStart, rayDir);
    rayLength = min(rayLength, intersection.y);
    if (intersection.x > 0) {
        // Advance ray to the atmosphere entry point
        rayStart += rayDir * intersection.x;
        rayLength -= intersection.x;
    }

    float costh = dot(rayDir, lightDir);
    float phaseR = phaseRayleigh(costh);
    float phaseM = phaseMie(costh);

    float3 opticalDepth = 0;
    float3 rayleigh = 0;
    float3 mie = 0;

    float prevRayTime = 0;
    for (int i = 0; i < skyData.scatteringSamples; ++i)
    {
        float rayTime = pow(float(i + 1) / skyData.scatteringSamples, sampleDistributionExponent) * rayLength;
        float stepSize = rayTime - prevRayTime;
        float sampleTime = (prevRayTime + rayTime) * 0.5;
        float3 localPosition = rayStart + rayDir * sampleTime;

        float localHeight = atmosphereHeight(localPosition);
        float3 localDensity = atmosphereDensity(localHeight);
        opticalDepth += localDensity * stepSize;

        // The atmospheric transmittance from rayStart to localPosition
        float3 viewTransmittance = absorb(opticalDepth, skyData);

        // The atmospheric transmittance of light reaching localPosition
        float3 lightTransmittance = transmittanceToLight(localPosition, lightDir, skyData);

        rayleigh += viewTransmittance * lightTransmittance * phaseR * localDensity.x * stepSize;
        mie += viewTransmittance * lightTransmittance * phaseM * localDensity.y * stepSize;

        prevRayTime = rayTime;
    }

    transmittance = absorb(opticalDepth, skyData);
    return (rayleigh * C_RAYLEIGH + mie * C_MIE) * lightColor * intensity;
}

public float3 calculateLightSource(
    float3 rayDir,
    float3 lightDir,
    float3 transmittedLight,
    float radius,
    float glowWidth,
    float glowStrength,
    out float discMask
) {
    float costh = dot(rayDir, lightDir);
    float edge = cos(radius);
    float width = max(fwidth(costh), EPS);
    discMask = smoothstep(edge - width, edge + width, costh);

    float angle = acos(clamp(costh, -1.0, 1.0));
    float distanceFromDisc = max(angle - radius, 0.0);
    float glow = exp(-distanceFromDisc * distanceFromDisc / (glowWidth * glowWidth));

    return (discMask + glow * glowStrength) * transmittedLight;
}

float3 starHash(float3 p) {
    float3 q = float3(
        dot(p, float3(127.1, 311.7, 74.7)),
        dot(p, float3(269.5, 183.3, 246.1)),
        dot(p, float3(113.5, 271.9, 124.6))
    );
    return frac(sin(q) * 43758.5453);
}

public float3 calculateStars(float3 direction, ProceduralSkyData skyData) {
    float3 p = direction * skyData.starScale;
    float3 cell = floor(p);
    float3 local = frac(p);

    float footprint = max(length(ddx(p)), length(ddy(p)));

    float coreWidth = skyData.starCoreWidth;
    float coreVariance = coreWidth * coreWidth;
    float filteredVariance = coreVariance + footprint * footprint / 12.0;

    float stars = 0.0;
    for (int z = -1; z <= 1; ++z) {
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                float3 offset = float3(x, y, z);
                float3 id = cell + offset;
                float3 attributes = starHash(id + 23.0);

                if (attributes.x > skyData.starDensity) {
                    continue;
                }

                float3 point = starHash(id);
                float3 delta = offset + point - local;
                float distanceSquared = dot(delta, delta);

                float core = exp(-distanceSquared / (2.0 * filteredVariance)) * coreVariance / filteredVariance;
                float glow = exp(-distanceSquared / (2.0 * skyData.starGlowWidth * skyData.starGlowWidth));

                float brightness =
                    skyData.starBrightness *
                    (
                        skyData.starMinBrightness +
                     skyData.starBrightnessVariation *
                         pow(attributes.y, skyData.starBrightnessFalloff)
                    );

                stars += brightness * (core + glow * skyData.starGlowStrength);
            }
        }
    }

    return float3(stars);
}

public float3 calculateSky(
    float3 origin,
    float3 worldDirection,
    EnvironmentData environmentData,
    ProceduralSkyData skyData
) {
    float3 sunDirection = normalize(-environmentData.lightDirection);
    float3 moonDirection = -sunDirection;
    float3 sunLight = environmentData.lightColor.rgb * environmentData.lightIntensity;
    float3 transmittance;
    float3 atmosphere = calculateScatteringAtmosphere(
        origin,
        worldDirection,
        INFINITY,
        sunDirection,
        environmentData.lightColor.rgb,
        environmentData.lightIntensity,
        skyData,
        transmittance
    );
    float3 unusedTransmittance;
    float3 moonAtmosphere = calculateScatteringAtmosphere(
        origin,
        worldDirection,
        INFINITY,
        moonDirection,
        environmentData.moonLight,
        1.0,
        skyData,
        unusedTransmittance
    );

    float sunDisc;
    float3 sun = calculateLightSource(
        worldDirection,
        sunDirection,
        sunLight,
        skyData.sunRadius,
        skyData.sunGlowWidth,
        skyData.sunGlowStrength,
        sunDisc
    ) * transmittance;
    float moonDisc;
    float3 moon = calculateLightSource(
        worldDirection,
        moonDirection,
        environmentData.moonRadiance,
        skyData.moonRadius,
        skyData.moonGlowWidth,
        skyData.moonGlowStrength,
        moonDisc
    ) * transmittance;

    float starSpeed = 3.0;
    float3 rotationAxis = environmentData.starAxis.xyz;
    float3 orbitUp = float3(0.0, 1.0, 0.0);
    float3 orbitForward = normalize(cross(rotationAxis, orbitUp));
    float angle = atan2(
        dot(moonDirection, orbitUp),
        dot(moonDirection, orbitForward)
    ) * starSpeed;
    float3 equatorX = orbitForward * cos(angle) + orbitUp * sin(angle);
    float3 equatorY = cross(rotationAxis, equatorX);
    float3 starDirection = float3(
        dot(worldDirection, equatorX),
        dot(worldDirection, equatorY),
        dot(worldDirection, rotationAxis)
    );
    float3 up = normalize(origin - PLANET_CENTER);
    float sunHeight = dot(up, sunDirection);
    float nightVisibility = 1.0 - smoothstep(sin(radians(-12.0)), sin(radians(-4.0)), sunHeight);
    float3 stars = calculateStars(starDirection, skyData) * nightVisibility * transmittance * (1.0 - moonDisc);

    float3 color = atmosphere + moonAtmosphere + sun + moon + stars;
    return color;
}
