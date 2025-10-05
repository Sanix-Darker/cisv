import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import path from 'path';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const require = createRequire(import.meta.url);

const gyp = require('node-gyp-build');
const addon = gyp(path.join(__dirname, '..'));

export const cisvParser = addon.cisvParser;
export default addon;
