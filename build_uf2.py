import sys
import struct

UF2_HEADER_FORMAT = "<IIIIIIII"
UF2_HEADER_SIZE = 32
UF2_BLOCK_SIZE = 512
UF2_PAYLOAD_SIZE = 256
UF2_MAGIC_START = 0x0A324655
UF2_MAGIC_END   = 0x0AB16F30  # Typical end-of-block UF2 marker

# Range to "erase" by creating zero-filled UF2 blocks.
ERASE_START_ADDRESS = 0x101E0000
ERASE_END_ADDRESS   = 0x101FFFFF

class UF2Block:
    def __init__(self, data: bytes):
        self.data = bytearray(data)
        (self.start_magic,
         self.magic2,
         self.flags,
         self.target_addr,
         self.payload_size,
         self.block_num,
         self.num_blocks,
         self.file_container) = struct.unpack(UF2_HEADER_FORMAT, self.data[:UF2_HEADER_SIZE])

    def is_valid_uf2(self) -> bool:
        return self.start_magic == UF2_MAGIC_START

    def update_header(self, block_num=None, num_blocks=None):
        if block_num is not None:
            self.block_num = block_num
        if num_blocks is not None:
            self.num_blocks = num_blocks
        struct.pack_into(
            UF2_HEADER_FORMAT,
            self.data,
            0,
            self.start_magic,
            self.magic2,
            self.flags,
            self.target_addr,
            self.payload_size,
            self.block_num,
            self.num_blocks,
            self.file_container
        )

# Helper to read an entire UF2 file into a list of UF2Block objects.
def read_uf2_blocks(filename: str) -> list[UF2Block]:
    with open(filename, "rb") as f:
        file_data = f.read()

    blocks = []
    total = len(file_data) // UF2_BLOCK_SIZE
    for i in range(total):
        chunk = file_data[i*UF2_BLOCK_SIZE : (i+1)*UF2_BLOCK_SIZE]
        blk = UF2Block(chunk)
        if blk.is_valid_uf2():
            blocks.append(blk)
    return blocks

# Helper to write out a list of UF2Block objects to a single UF2 file.
def write_uf2_blocks(blocks: list[UF2Block], filename: str):
    with open(filename, "wb") as f:
        for blk in blocks:
            f.write(blk.data)

# Create new UF2 blocks covering [start_addr, end_addr] with zeroed payload.
# This effectively "erases" the region in the final combined UF2.
# We'll assign a placeholder block_num=0, num_blocks=0 here.
# We'll fix them later in a final pass.
def create_zeroed_region_blocks(start_addr: int, end_addr: int, flags=0, file_container=0) -> list[UF2Block]:
    blocks = []
    size = (end_addr - start_addr + 1)
    offset = 0

    while size > 0:
        chunk_size = min(UF2_PAYLOAD_SIZE, size)

        block_data = bytearray(UF2_BLOCK_SIZE)
        # Header
        struct.pack_into(
            UF2_HEADER_FORMAT,
            block_data,
            0,
            UF2_MAGIC_START,
            0x9E5D5157,     # magic2 typical
            flags,          # flags
            start_addr + offset,
            chunk_size,
            0,              # block_num placeholder
            0,              # num_blocks placeholder
            file_container  # file_container placeholder
        )

        # For a standard UF2 block, we also place the end-of-block marker
        struct.pack_into('<I', block_data, 508, UF2_MAGIC_END)

        # No need to fill the payload with anything other than zeros.
        # By default it's already zero.

        new_blk = UF2Block(block_data)
        blocks.append(new_blk)

        offset += chunk_size
        size   -= chunk_size

    return blocks


def main():
    if len(sys.argv) != 4:
        print("Usage: python build_uf2.py <input1.uf2> <input2.uf2> <output.uf2>")
        sys.exit(1)

    input1 = sys.argv[1]
    input2 = sys.argv[2]
    output = sys.argv[3]

    # 1) Read both UF2 files as blocks.
    uf2_a = read_uf2_blocks(input1)
    uf2_b = read_uf2_blocks(input2)

    # 2) Combine blocks from both inputs in a simple list.
    #    We'll fix the block numbering afterward.
    combined = uf2_a + uf2_b

    # 3) Create the zeroed region for [ERASE_START_ADDRESS, ERASE_END_ADDRESS].
    #    We'll treat it as if it was its own UF2 file (i.e. appended after all else).
    erase_region_blocks = create_zeroed_region_blocks(
        ERASE_START_ADDRESS,
        ERASE_END_ADDRESS,
        flags=uf2_a[0].flags,                     # or use uf2_a[0].flags if you prefer using the same flags from the first file
        file_container=uf2_a[0].file_container    # likewise
    )

    # 4) Append those new blocks at the end.
    combined += erase_region_blocks

    # 5) Now finalize block numbering. Each block gets a distinct block_num.
    #    Also set num_blocks in every header to the total.
    total_blocks = len(combined)
    for i, blk in enumerate(combined):
        blk.update_header(block_num=i, num_blocks=total_blocks)

    # 6) Write the final UF2.
    write_uf2_blocks(combined, output)

    print(f"Combined {len(uf2_a)} blocks + {len(uf2_b)} blocks + {len(erase_region_blocks)} blocks from erase-region = {total_blocks} total.")
    print(f"Zeroed region appended as blocks from 0x{ERASE_START_ADDRESS:08X} to 0x{ERASE_END_ADDRESS:08X}.")
    print("Done.")


if __name__ == "__main__":
    main()
