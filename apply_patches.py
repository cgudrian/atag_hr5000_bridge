Import("env")
import os
import shutil

def apply_patches(source, target, env):
    """Apply patches to fix ArduinoJson deprecation warnings in ESP8266 IoT Framework"""
    project_dir = env.get("PROJECT_DIR", ".")
    libdeps_base = os.path.join(project_dir, ".pio", "libdeps")
    
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
                    if 'StaticJsonDocument' in content or 'createNestedArray(' in content:
                        print("Applying ArduinoJson deprecation fixes to ESP8266 IoT Framework...")
                        
                        # Apply the fixes
                        content = content.replace('StaticJsonDocument<200>', 'JsonDocument')
                        content = content.replace('StaticJsonDocument<1000>', 'JsonDocument') 
                        content = content.replace('StaticJsonDocument<100>', 'JsonDocument')
                        content = content.replace('jsonBuffer.createNestedArray("files")', 'jsonBuffer["files"].to<JsonArray>()')
                        
                        # Write the patched file
                        with open(webserver_file, 'w') as f:
                            f.write(content)
                        
                        print("ArduinoJson deprecation fixes applied successfully!")
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