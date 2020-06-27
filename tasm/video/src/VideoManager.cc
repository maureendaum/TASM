#include "VideoManager.h"

#include "ImageUtilities.h"
#include "MergeTiles.h"
#include "TileLocationProvider.h"
#include "TiledVideoManager.h"
#include "ScanOperators.h"
#include "ScanTiledVideoOperator.h"
#include "DecodeOperators.h"
#include "SemanticIndex.h"
#include "SemanticSelection.h"
#include "SmartTileConfigurationProvider.h"
#include "TemporalSelection.h"
#include "TileOperators.h"
#include "TransformToImage.h"
#include "Video.h"
#include "VideoConfiguration.h"
#include "WorkloadCostEstimator.h"


namespace tasm {

void VideoManager::createCatalogIfNecessary() {
    if (!std::experimental::filesystem::exists(CatalogPath))
        std::experimental::filesystem::create_directory(CatalogPath);
}

void VideoManager::store(const std::experimental::filesystem::path &path, const std::string &name) {
    std::shared_ptr<Video> video(new Video(path));
    auto tileConfigurationProvider = std::make_shared<SingleTileConfigurationProvider>(
            video->configuration().displayWidth,
            video->configuration().displayHeight);
    storeTiledVideo(video, tileConfigurationProvider, name);
}

void VideoManager::storeWithUniformLayout(const std::experimental::filesystem::path &path, const std::string &name, unsigned int numRows, unsigned int numColumns) {
    std::shared_ptr<Video> video(new Video(path));
    auto tileConfigurationProvider = std::make_shared<UniformTileconfigurationProvider>(numRows, numColumns, video->configuration());
    storeTiledVideo(video, tileConfigurationProvider, name);
}

void VideoManager::storeWithNonUniformLayout(const std::experimental::filesystem::path &path,
                                                const std::string &storedName,
                                                const std::string &metadataIdentifier,
                                                std::shared_ptr<MetadataSelection> metadataSelection,
                                                std::shared_ptr<SemanticIndex> semanticIndex, bool force) {
    std::shared_ptr<Video> video(new Video(path));
    auto semanticDataManager = std::make_shared<SemanticDataManager>(semanticIndex, metadataIdentifier, metadataSelection, std::shared_ptr<TemporalSelection>());
    std::shared_ptr<TileLayoutProvider> layoutProvider;

    auto layoutDuration = video->configuration().frameRate;
    auto width = video->configuration().displayWidth;
    auto height = video->configuration().displayHeight;

    if (force) {
        layoutProvider = std::make_shared<FineGrainedTileConfigurationProvider>(
                layoutDuration,
                semanticDataManager,
                width,
                height);
    } else {
        layoutProvider = std::make_shared<SmartTileConfigurationProviderSingleSelection>(
                layoutDuration,
                semanticDataManager,
                width,
                height);
    }
    storeTiledVideo(video, layoutProvider, storedName);
}

void VideoManager::storeTiledVideo(std::shared_ptr<Video> video, std::shared_ptr<TileLayoutProvider> tileLayoutProvider, const std::string &savedName) {
    std::shared_ptr<ScanFileDecodeReader> scan(new ScanFileDecodeReader(video));
    std::shared_ptr<GPUDecodeFromCPU> decode(new GPUDecodeFromCPU(scan, video->configuration(), gpuContext_, lock_));

    TileOperator tile(video, decode, tileLayoutProvider, savedName, video->configuration().frameRate, gpuContext_, lock_);
    while (!tile.isComplete()) {
        tile.next();
    }
}

std::unique_ptr<ImageIterator> VideoManager::select(const std::string &video,
                                                     const std::string &metadataIdentifier,
                                                     std::shared_ptr<MetadataSelection> metadataSelection,
                                                     std::shared_ptr<TemporalSelection> temporalSelection,
                                                     std::shared_ptr<SemanticIndex> semanticIndex) {
    std::shared_ptr<TiledEntry> entry(new TiledEntry(video, metadataIdentifier));

    // Set up scan of a tiled video.
    std::shared_ptr<TiledVideoManager> tiledVideoManager(new TiledVideoManager(entry));
    auto tileLocationProvider = std::make_shared<SingleTileLocationProvider>(tiledVideoManager);
    auto semanticDataManager = std::make_shared<SemanticDataManager>(semanticIndex, metadataIdentifier, metadataSelection, temporalSelection, tiledVideoManager->totalWidth(), tiledVideoManager->totalHeight());
    auto scan = std::make_shared<ScanTiledVideoOperator>(entry, semanticDataManager, tileLocationProvider);

    // Set up decode. Specify largest tile dimensions which are required to successfully reconfigure the decoder.
    auto maxWidth = tiledVideoManager->largestWidth();
    auto maxHeight = tiledVideoManager->largestHeight();

    // The maximum dimensions were set based on display dimensions; make sure they are big enough to handle larger coded dimensions.
    static const unsigned int CodedDimension = 32;
    if (maxWidth % CodedDimension)
        maxWidth = (maxWidth / CodedDimension + 1) * CodedDimension;
    if (maxHeight % CodedDimension)
        maxHeight = (maxHeight / CodedDimension + 1) * CodedDimension;

    auto configuration = video::GetConfiguration(tileLocationProvider->locationOfTileForFrame(0, 0));
    configuration->maxWidth = maxWidth;
    configuration->maxHeight = maxHeight;

    std::shared_ptr<GPUDecodeFromCPU> decode(new GPUDecodeFromCPU(scan, *configuration, gpuContext_, lock_, maxWidth, maxHeight));

    // Set up tile merger.
    std::shared_ptr<MergeTilesOperator> merge(new MergeTilesOperator(decode, semanticDataManager, tileLocationProvider));

    // Transform pixels to RGB images.
    std::shared_ptr<TransformToImage> transform(new TransformToImage(merge, maxWidth, maxHeight));

    return std::make_unique<ImageIterator>(transform);
}

} // namespace tasm