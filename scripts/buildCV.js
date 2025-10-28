const os = require('os');
const { exec } = require('child_process');

// Get the build type from command line arguments
const buildType = process.argv[2]?.toLowerCase(); // e.g., 'debug' or 'release'

if (!buildType || !['debug', 'release'].includes(buildType)) {
    console.error('Usage: node build.js <debug|release>');
    process.exit(1);
}

// Detect current OS
const platform = os.platform();
const currentOS = (platform === 'win32') ? 'Windows' : 'Unix';

// Define presets for each OS
const presets = {
    Windows: {
        debug: 'windows-debug',
        release: 'windows-release'
    },
    Unix: {
        debug: 'unix-debug',
        release: 'unix-release'
    }
};

// Function to run a command and return a Promise
function runCommand(cmd) {
    return new Promise((resolve, reject) => {
        exec(cmd, (error, stdout, stderr) => {
            if (error) {
                reject(`Error: ${error.message}`);
                return;
            }
            if (stderr) {
                console.error(`Stderr: ${stderr}`);
            }
            console.log(stdout);
            resolve();
        });
    });
}

// Function to configure and build a preset
async function configureAndBuild(preset) {
    console.log(`\n=== Configuring preset: ${preset} ===`);
    await runCommand(`cmake --preset ${preset}`);

    console.log(`\n=== Building preset: ${preset} ===`);
    await runCommand(`cmake --build --preset ${preset}`);
}

// Main
async function main() {
    try {
        const preset = presets[currentOS][buildType];
        await configureAndBuild(preset);
        console.log(`\n${buildType} build completed successfully.`);
    } catch (err) {
        console.error(`\nBuild failed: ${err}`);
    }
}

main();
