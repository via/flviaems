import cbor
import io
import struct
import sqlite3
import sys
import zstd

infilepath = sys.argv[1]
outfile = open('out.viaemslog', 'wb')
outfile.write(b"VIAEMSLOG1")

indb = sqlite3.connect(infilepath)
cursor = indb.cursor()

indexes = []
this_chunk = []

def _pack_uint8(v):  return struct.pack('<B', v)
def _pack_uint32(v): return struct.pack('<L', v)
def _pack_float32(v): return struct.pack('<f', v)
def _pack_uint64(v): return struct.pack('<Q', v)

def write_chunk(values):
    
    columns = []
    for name, value in values[0]:
        if name == 'realtime_ns':
            continue
        if type(value) == float:
            columns.append({"name": name, "type": "float"})
        if type(value) == int:
            columns.append({"name": name, "type": "uint32"})
    header = {
        "chunk_type": "data",
        "name": "feed",
        "compression": "zstd",
        "columns": columns, }
    headercbor = cbor.dumps(header)

    databuf = io.BytesIO()
    for row in values:
        for key, value in row:
            if key == 'realtime_ns':
                databuf.write(_pack_uint64(value))
            elif type(value) == int:
                databuf.write(_pack_uint32(value))
            elif type(value) == float:
                databuf.write(_pack_float32(value))

    data = zstd.compress(databuf.getvalue())
#    data = databuf.getvalue()

    chunksize = 17 + len(headercbor) + len(data)
    offset = outfile.tell()
    outfile.write(_pack_uint64(chunksize))
    outfile.write(_pack_uint8(0x1))
    outfile.write(_pack_uint64(len(headercbor)))
    outfile.write(headercbor)
    outfile.write(data)

    start_time = values[0][0][1]
    stop_time = values[-1][0][1]
    print(f"Chunk spans {(stop_time - start_time) / 1000000} ms in {len(values)} values")

    size = chunksize
    indexes.append((start_time, stop_time, offset, size))

def write_meta():
    chunklist = io.BytesIO()
    for i in indexes:
        chunklist.write(_pack_uint64(i[0]))
        chunklist.write(_pack_uint64(i[1]))
        chunklist.write(_pack_uint8(0x01))
        chunklist.write(_pack_uint64(i[2]))
        chunklist.write(_pack_uint64(i[3]))
    chunklistbytes = chunklist.getvalue()
    size = 8 + 1 + 8 + 8 + len(chunklistbytes) + 8 + 4

    outfile.write(_pack_uint64(size))
    outfile.write(_pack_uint8(0x03))
    outfile.write(_pack_uint64(0))
    outfile.write(_pack_uint64(len(indexes)))
    outfile.write(chunklistbytes)
    outfile.write(_pack_uint64(size))
    outfile.write(b"meta")

for point in cursor.execute("SELECT * from points order by realtime_ns asc"):
  values = list(zip([x[0] for x in cursor.description], point))
  this_chunk.append(values)
  if len(this_chunk) > 100000:
    write_chunk(this_chunk)
    this_chunk = []
    print(len(indexes))
    if len(indexes) > 100:
        write_meta()
        outfile.close()
        break



