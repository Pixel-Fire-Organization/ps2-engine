#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path

def main():
    # Resolve the directory of this script
    script_dir = Path(__file__).resolve().parent
    
    # The generator project directory relative to the script
    generator_dir = script_dir / "ECS" / "ECSGenerator"
    
    if not generator_dir.exists():
        print(f"Error: Generator directory does not exist: {generator_dir}", file=sys.stderr)
        sys.exit(1)

    # Command arguments exactly as requested
    command = [
        "dotnet",
        "run",
        "../schema.json",
        "../ECS.json",
        "../../../engine/include/ecs/Components.h",
        "../../../tools/trenchbroom/games/PS2Engine/PS2Engine.fgd"
    ]

    print(f"Running ECS generator in {generator_dir}...")
    print(f"Command: {' '.join(command)}")

    try:
        # Run the command with generator_dir as the current working directory
        subprocess.run(
            command,
            cwd=generator_dir,
            check=True,
            text=True
        )
        print("ECS generated successfully.")
    except subprocess.CalledProcessError as e:
        print(f"Error: ECS generation failed with exit code {e.returncode}.", file=sys.stderr)
        sys.exit(e.returncode)
    except FileNotFoundError:
        print("Error: 'dotnet' command not found. Please ensure the .NET SDK is installed and in your PATH.", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
