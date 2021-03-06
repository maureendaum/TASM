#ifndef HOMOMORPHIC_STITCHING_GOLOMBS_H
#define HOMOMORPHIC_STITCHING_GOLOMBS_H

#include "BitArray.h"

namespace stitching {
    class BitStream;

    /**
     * Encodes the values in golombs as golombs, in the order provided
     * @param ints The integers to be encoded as ints
     * @returns A BitArray holding the encoded contents
     */
    BitArray EncodeGolombs(const std::vector<unsigned long> &golombs);

    /**
     * Decodes the next golomb in the stream, returning the value as a long
     * @param stream The stream that contains the bits of the golomb, with its iterator
     * set to the first bit
     * @return The golomb
     */
    unsigned long DecodeGolomb(BitStream &stream);

    BitArray EncodeGolombWithSize(unsigned long value, unsigned long size);
}; //namespace stitching

#endif //HOMOMORPHIC_STITCHING_GOLOMBS_H
