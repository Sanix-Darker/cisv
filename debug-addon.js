// debug-addon.js
console.log('=== Debugging Node.js Addon ===\n');

try {
    console.log('1. Attempting to load the addon...');
    const addon = require('./build/Release/'); // or your main entry point
    console.log('✓ Addon loaded successfully');
    console.log('   Addon exports:', Object.keys(addon));

    if (!addon.cisvParser) {
        console.error('✗ cisvParser class not found in exports');
        console.log('   Available exports:', addon);
        process.exit(1);
    }

    console.log('\n2. Creating parser instance...');
    const parser = new addon.cisvParser();
    console.log('✓ Parser instance created');

    console.log('\n3. Checking available methods...');
    const prototype = Object.getPrototypeOf(parser);
    const methods = Object.getOwnPropertyNames(prototype);
    console.log('   Available methods:', methods);

    console.log('\n4. Checking transform method specifically...');
    console.log('   transform exists:', 'transform' in parser);
    console.log('   transform type:', typeof parser.transform);

    if (typeof parser.transform === 'function') {
        console.log('✓ transform method is available and is a function');

        console.log('\n5. Testing basic transform call...');
        try {
            // Test with minimal arguments
            const result = parser.transform(0, 'uppercase');
            console.log('✓ transform method call succeeded');
            console.log('   Result:', result);
        } catch (err) {
            console.error('✗ transform method call failed:', err.message);
            console.error('   Stack:', err.stack);
        }
    } else {
        console.error('✗ transform method is not a function');
        console.log('   Actual type:', typeof parser.transform);
        console.log('   Value:', parser.transform);
    }

    console.log('\n6. Checking other methods...');
    ['parseSync', 'write', 'end', 'getRows', 'transformField', 'transformRow', 'transformSchema'].forEach(method => {
        console.log(`   ${method}: ${typeof parser[method]}`);
    });

} catch (error) {
    console.error('✗ Failed to load or test addon:', error.message);
    console.error('Full error:', error);

    console.log('\nTroubleshooting steps:');
    console.log('1. Check if addon compiled: ls -la build/Release/');
    console.log('2. Try rebuilding: npm rebuild or node-gyp rebuild');
    console.log('3. Check your index.js exports the right module');
    console.log('4. Verify binding.gyp configuration');
}

console.log('\n=== Debug Complete ===');
