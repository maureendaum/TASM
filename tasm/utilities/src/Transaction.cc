#include "Transaction.h"

#include "Gpac.h"
#include <iostream>

void TileCrackingTransaction::prepareTileDirectory() {
    auto directory = tasm::TileFiles::directoryForTilesInFrames(*entry_, firstFrame_, lastFrame_);
    std::error_code error;
    if (!std::experimental::filesystem::create_directory(directory, error))
        std::cerr << "Failed to create tile directory: " << error.message() << std::endl;
}

void TileCrackingTransaction::abort() {
    complete_ = true;
    for(const auto &output: outputs())
        std::experimental::filesystem::remove(output.filename());
}

void TileCrackingTransaction::commit() {
    complete_ = true;

    for (auto &output : outputs()) {
        output.stream().close();

        // Mux the outputs to mp4.
        auto muxedFile = output.filename();
        muxedFile.replace_extension(tasm::TileFiles::muxedFilenameExtension());
        tasm::gpac::mux_media(output.filename(), muxedFile);
    }

    writeTileMetadata();

    entry_->incrementTileVersion();
}

void TileCrackingTransaction::writeTileMetadata() {
    std::shared_ptr<tasm::TileLayout> layout = std::make_shared<tasm::TileLayout>(tileLayout_);
    tasm::LayoutDatabase::instance()->addTileLayout(entry_->name(), entry_->tile_version(), (unsigned int)firstFrame_, (unsigned int)lastFrame_, layout);
}