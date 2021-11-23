// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// @file   EntropyEncoderSpec.cxx
/// @author Michael Lettrich, Matthias Richter
/// @since  2020-01-16
/// @brief  ProcessorSpec for the TPC cluster entropy encoding

#include "TPCWorkflow/EntropyEncoderSpec.h"
#include "DataFormatsTPC/CompressedClusters.h"
#include "Framework/ConfigParamRegistry.h"
#include "Headers/DataHeader.h"

using namespace o2::framework;
using namespace o2::header;

namespace o2
{
namespace tpc
{

void EntropyEncoderSpec::init(o2::framework::InitContext& ic)
{
  mCTFCoder.setCombineColumns(!ic.options().get<bool>("no-ctf-columns-combining"));
  mCTFCoder.setMemMarginFactor(ic.options().get<float>("mem-factor"));
  std::string dictPath = ic.options().get<std::string>("ctf-dict");
  if (!dictPath.empty() && dictPath != "none") {
    mCTFCoder.createCoders(dictPath, o2::ctf::CTFCoderBase::OpType::Encoder);
  }
}

void EntropyEncoderSpec::run(ProcessingContext& pc)
{
  CompressedClusters clusters;

  if (mFromFile) {
    auto tmp = pc.inputs().get<CompressedClustersROOT*>("input");
    if (tmp == nullptr) {
      LOG(error) << "invalid input";
      return;
    }
    clusters = *tmp;
  } else {
    auto tmp = pc.inputs().get<CompressedClustersFlat*>("input");
    if (tmp == nullptr) {
      LOG(error) << "invalid input";
      return;
    }
    clusters = *tmp;
  }
  auto cput = mTimer.CpuTime();
  mTimer.Start(false);

  auto& buffer = pc.outputs().make<std::vector<o2::ctf::BufferType>>(Output{"TPC", "CTFDATA", 0, Lifetime::Timeframe});
  mCTFCoder.encode(buffer, clusters);
  auto encodedBlocks = CTF::get(buffer.data()); // cast to container pointer
  encodedBlocks->compactify();                  // eliminate unnecessary padding
  buffer.resize(encodedBlocks->size());         // shrink buffer to strictly necessary size
  // encodedBlocks->print();
  mTimer.Stop();
  LOG(info) << "Created encoded data of size " << encodedBlocks->size() << " for TPC in " << mTimer.CpuTime() - cput << " s";
}

void EntropyEncoderSpec::endOfStream(EndOfStreamContext& ec)
{
  LOGF(INFO, "TPC Entropy Encoding total timing: Cpu: %.3e Real: %.3e s in %d slots",
       mTimer.CpuTime(), mTimer.RealTime(), mTimer.Counter() - 1);
}

DataProcessorSpec getEntropyEncoderSpec(bool inputFromFile)
{
  std::vector<InputSpec> inputs;
  header::DataDescription inputType = inputFromFile ? header::DataDescription("COMPCLUSTERS") : header::DataDescription("COMPCLUSTERSFLAT");
  return DataProcessorSpec{
    "tpc-entropy-encoder", // process id
    Inputs{{"input", "TPC", inputType, 0, Lifetime::Timeframe}},
    Outputs{{"TPC", "CTFDATA", 0, Lifetime::Timeframe}},
    AlgorithmSpec{adaptFromTask<EntropyEncoderSpec>(inputFromFile)},
    Options{{"ctf-dict", VariantType::String, o2::base::NameConf::getCTFDictFileName(), {"File of CTF encoding dictionary"}},
            {"no-ctf-columns-combining", VariantType::Bool, false, {"Do not combine correlated columns in CTF"}},
            {"mem-factor", VariantType::Float, 1.f, {"Memory allocation margin factor"}}}};
}

} // namespace tpc
} // namespace o2
