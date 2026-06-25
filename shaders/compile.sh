#!/bin/bash

# Target profile (e.g., spirv, dxil, glsl, hlsl)
TARGET_PROFILE="spirv"
OUTPUT_EXT="spv"

for shader in *.slang; do
    # Check if files exist to prevent literal "*.slang" execution
    [ -e "$shader" ] || continue

    base_name="${shader%.slang}"
    output_file="${base_name}.${OUTPUT_EXT}"

    echo "Compiling $shader -> $output_file..."
    
    # Adjust -profile if your shaders use specific entry points (e.g., -profile cs_6_5)
    slangc "$shader" -target "$TARGET_PROFILE" -o "$output_file"

    if [ $? -eq 0 ]; then
        echo "Successfully compiled $base_name"
    else
        echo "Error compiling $shader" >&2
    fi
done
