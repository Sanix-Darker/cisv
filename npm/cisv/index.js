const path = require('path');
const addon = require('node-gyp-build')(path.join(__dirname, '..'));

module.exports = addon;
