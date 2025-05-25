Import("env")
import os
import subprocess

def apply_patches(source, target, env):
    """Apply patches to fix ArduinoJson deprecation warnings in ESP8266 IoT Framework"""
    project_dir = env.get("PROJECT_DIR", ".")
    libdeps_base = os.path.join(project_dir, ".pio", "libdeps")
    patch_file = os.path.join(project_dir, "patches", "esp8266-iot-framework-fix-arduinojson.patch")
    
    if not os.path.exists(patch_file):
        return
    
    if os.path.exists(libdeps_base):
        for env_name in os.listdir(libdeps_base):
            env_path = os.path.join(libdeps_base, env_name)
            framework_path = os.path.join(env_path, "ESP8266 IoT Framework")
            if os.path.exists(framework_path):
                webserver_file = os.path.join(framework_path, "src", "webServer.cpp")
                
                if os.path.exists(webserver_file):
                    # Read the file to check if it needs patching
                    with open(webserver_file, 'r') as f:
                        content = f.read()
                    
                    # Only patch if we find deprecated code
                    if 'StaticJsonDocument' in content:
                        print("Applying ArduinoJson deprecation fixes to ESP8266 IoT Framework...")
                        
                        # Apply the patch file
                        try:
                            result = subprocess.run([
                                'patch', '-p1', '-d', framework_path, '-i', patch_file
                            ], capture_output=True, text=True, check=True)
                            print("ArduinoJson deprecation fixes applied successfully!")
                        except subprocess.CalledProcessError as e:
                            print(f"Patch failed: {e}")
                            print(f"stdout: {e.stdout}")
                            print(f"stderr: {e.stderr}")
                        break

# Apply patches before any compilation starts
def patch_before_build(env):
    apply_patches(None, None, env)

env.AddPostAction("$BUILD_DIR", patch_before_build)

# Also try to run early in build process
try:
    patch_before_build(env)
except:
    pass