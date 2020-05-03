const { Transform } = require('stream')

class DelimiterParser extends Transform {
  constructor(options = {}) {
    super(options)

    if (options.delimiter === undefined) {
      throw new TypeError('"delimiter" is not a bufferable object')
    }

    if (options.delimiter.length === 0) {
      throw new TypeError('"delimiter" has a 0 or undefined length')
    }

    this.includeDelimiter =
      options.includeDelimiter !== undefined ? options.includeDelimiter : false
    this.delimiter = Buffer.from(options.delimiter)
    this.buffer = Buffer.alloc(0)
  }

  _transform(chunk, encoding, cb) {
    let data = Buffer.concat([this.buffer, chunk])
    let position

    while ((position = data.indexOf(this.delimiter)) !== -1) {
      this.push(
        data.slice(
          0,
          position + (this.includeDelimiter ? this.delimiter.length : 0)
        )
      )
      data = data.slice(position + this.delimiter.length)
    }
    this.buffer = data

    cb()
  }

  _flush(cb) {
    this.push(this.buffer)
    this.buffer = Buffer.alloc(0)
    cb()
  }
}

class ReadLineParser extends DelimiterParser {
  constructor(options) {
    const opts = {
      delimiter: Buffer.from('\n', 'utf8'),
      encoding: 'utf8',
      ...options,
    }

    if (typeof opts.delimiter === 'string') {
      opts.delimiter = Buffer.from(opts.delimiter, opts.encoding)
    }

    super(opts)
  }
}

module.exports = ReadLineParser
