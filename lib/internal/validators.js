'use strict';

const {
  hideStackFrames,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OUT_OF_RANGE
  }
} = require('internal/errors');

function isInt32(value) {
  return value === (value | 0);
}

function isUint32(value) {
  return value === (value >>> 0);
}

const octalReg = /^[0-7]+$/;
const modeDesc = 'must be a 32-bit unsigned integer or an octal string';

/**
 * Validate values that will be converted into mode_t (the S_* constants).
 * Only valid numbers and octal strings are allowed. They could be converted
 * to 32-bit unsigned integers or non-negative signed integers in the C++
 * land, but any value higher than 0o777 will result in platform-specific
 * behaviors.
 *
 * @param {*} value Values to be validated
 * @param {string} name Name of the argument
 * @param {number} def If specified, will be returned for invalid values
 * @returns {number}
 */
function validateMode(value, name, def) {
  if (isUint32(value)) {
    return value;
  }

  if (typeof value === 'number') {
    validateInt32(value, name, 0, 2 ** 32 - 1);
  }

  if (typeof value === 'string') {
    if (!octalReg.test(value)) {
      throw new ERR_INVALID_ARG_VALUE(name, value, modeDesc);
    }
    return parseInt(value, 8);
  }

  if (def !== undefined && value == null) {
    return def;
  }

  throw new ERR_INVALID_ARG_VALUE(name, value, modeDesc);
}

const validateInteger = hideStackFrames((value, name) => {
  if (typeof value !== 'number')
    throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
  if (!Number.isSafeInteger(value))
    throw new ERR_OUT_OF_RANGE(name, 'an integer', value);
  return value;
});

const validateInt32 = hideStackFrames(
  (value, name, min = -2147483648, max = 2147483647) => {
    // The defaults for min and max correspond to the limits of 32-bit integers.
    if (!isInt32(value)) {
      if (typeof value !== 'number') {
        throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
      }
      if (!Number.isInteger(value)) {
        throw new ERR_OUT_OF_RANGE(name, 'an integer', value);
      }
      throw new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
    }
    if (value < min || value > max) {
      throw new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
    }
    return value;
  }
);

const validateUint32 = hideStackFrames((value, name, positive) => {
  if (!isUint32(value)) {
    if (typeof value !== 'number') {
      throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
    }
    if (!Number.isInteger(value)) {
      throw new ERR_OUT_OF_RANGE(name, 'an integer', value);
    }
    const min = positive ? 1 : 0;
    // 2 ** 32 === 4294967296
    throw new ERR_OUT_OF_RANGE(name, `>= ${min} && < 4294967296`, value);
  }
  if (positive && value === 0) {
    throw new ERR_OUT_OF_RANGE(name, '>= 1 && < 4294967296', value);
  }
  // TODO(BridgeAR): Remove return values from validation functions and
  // especially reduce side effects caused by validation functions.
  return value;
});

function validateString(value, name) {
  if (typeof value !== 'string')
    throw new ERR_INVALID_ARG_TYPE(name, 'string', value);
}

function validateNumber(value, name) {
  if (typeof value !== 'number')
    throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
}

module.exports = {
  isInt32,
  isUint32,
  validateMode,
  validateInteger,
  validateInt32,
  validateUint32,
  validateString,
  validateNumber
};
