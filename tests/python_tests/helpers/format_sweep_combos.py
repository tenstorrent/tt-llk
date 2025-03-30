from .format_arg_mapping import *
import sys

formats = [DataFormat.Float32, DataFormat.Float16, DataFormat.Float16_b, DataFormat.Bfp8_b]
unpacker_out ={
    DataFormat.Float32: [DataFormat.Float32, DataFormat.Float16, DataFormat.Float16_b],
    DataFormat.Float16: [DataFormat.Float16],
    DataFormat.Float16_b: [DataFormat.Float16_b],
    DataFormat.Bfp8_b: [DataFormat.Bfp8_b],
}

unpacker_gasket_out = {
    DataFormat.Float32: [DataFormat.Float32], # if we unpack to dest and dest register in fp32_mode
    DataFormat.Float16: [DataFormat.Float16],
    DataFormat.Float16_b: [DataFormat.Float16_b],
    DataFormat.Bfp8_b: [DataFormat.Float16_b],
}

packer_gasket_out = {
    DataFormat.Float32: [DataFormat.Float32, DataFormat.Float16, DataFormat.Float16_b, DataFormat.Bfp8_b],
    DataFormat.Float16: [DataFormat.Float16],
    DataFormat.Float16_b: [DataFormat.Float16_b, DataFormat.Bfp8_b, DataFormat.Float16],
    DataFormat.Bfp8_b: [],
}

packer_out = {
    DataFormat.Float32: [DataFormat.Float32, DataFormat.Float16, DataFormat.Float16_b],
    DataFormat.Float16: [DataFormat.Float32, DataFormat.Float16, DataFormat.Float16_b],
    DataFormat.Float16_b: [DataFormat.Float32, DataFormat.Bfp8_b, DataFormat.Float16_b],
    DataFormat.Bfp8_b: [DataFormat.Float32, DataFormat.Float16_b, DataFormat.Bfp8_b],
}

def get_all_formats(fp32_mode):
    all_formats = []
    for in_format in formats:
        unpacker_src = in_format
        for out_format in unpacker_out[in_format]:
            unpacker_dst = out_format
            for dest_format in get_dest_format(unpacker_dst, fp32_mode):
                pack_src_formats = packer_gasket_out[dest_format]
                for pack_src in pack_src_formats:
                    for pack_dst in packer_out[pack_src]:
                        all_formats.append((unpacker_src, unpacker_dst, pack_src, pack_dst, DataFormat.Float16))
                        # all_formats.append([unpacker_src.name, unpacker_dst.name, pack_src.name, pack_dst.name])
                
    return all_formats

def get_dest_format(unpack_dst, fp32_mode):
    if fp32_mode == DestAccumulation.Yes:
        return unpacker_gasket_out[DataFormat.Float32]
    return unpacker_gasket_out[unpack_dst]

if __name__ == "__main__":
    all_formats = get_all_formats()
    for format_combo in all_formats:
        print(format_combo)
    sys.stdout.flush()

