import os
import json
import subprocess
import shlex

def generate_compile_commands():
    # Run make in dry-run mode to see all commands without actually building
    try:
        # --always-make (-B) forces all targets to be evaluated
        # --dry-run (-n) just prints the commands
        result = subprocess.run(
            ["make", "--always-make", "--dry-run"],
            capture_output=True,
            text=True,
            check=True
        )
    except subprocess.CalledProcessError as e:
        print("Error running make:")
        print(e.stderr)
        return

    compile_commands = []
    cwd = os.path.abspath(os.getcwd())

    # Process each line of the output
    for line in result.stdout.splitlines():
        line = line.strip()
        parts = shlex.split(line)
        if not parts:
            continue
            
        cmd_exec = parts[0]
        
        # Heuristic to detect compilation commands
        if ("g++" in cmd_exec or "clang++" in cmd_exec or "c++" in cmd_exec) and "-c" in parts:
            src_file = None
            
            # Find the source file (usually follows -c, or ends with .cpp)
            for i, part in enumerate(parts):
                if part == "-c" and i + 1 < len(parts):
                    src_file = parts[i+1]
                    break
            
            # Fallback check for source files
            if not src_file:
                for part in parts:
                    if part.endswith(".cpp") or part.endswith(".cc") or part.endswith(".c"):
                        src_file = part
                        break
                        
            if src_file:
                compile_commands.append({
                    "directory": cwd,
                    "command": line,
                    "file": os.path.join(cwd, src_file) if not os.path.isabs(src_file) else src_file
                })

    # Write out the JSON compilation database
    with open("compile_commands.json", "w") as f:
        json.dump(compile_commands, f, indent=2)
        
    print(f"Successfully generated compile_commands.json with {len(compile_commands)} entries.")

if __name__ == "__main__":
    generate_compile_commands()
