#include <color_spinor_field.h>
#include <tunable_nd.h>
#include <kernels/copy_color_spinor.cuh>

namespace quda {

  template <int Ns, int Nc, typename Out, typename In, typename param_t>
  class CopyColorSpinor : TunableKernel2D {
    using FloatOut = typename std::remove_pointer<typename std::tuple_element<0, param_t>::type>::type;
    using FloatIn = typename std::remove_pointer<typename std::tuple_element<1, param_t>::type>::type;
    template <template <int, int> class Basis> using Arg = CopyColorSpinorArg<FloatOut, FloatIn, Ns, Nc, Out, In, Basis>;
    FloatOut *Out_;
    FloatIn *In_;
    float *outNorm;
    float *inNorm;
    ColorSpinorField &out;
    const ColorSpinorField &in;

    bool advanceSharedBytes(TuneParam &param) const { return false; } // Don't tune shared mem
    unsigned int minThreads() const { return in.VolumeCB(); }

  public:
    CopyColorSpinor(ColorSpinorField &out, const ColorSpinorField &in, param_t &param) :
      TunableKernel2D(in, in.SiteSubset(), std::get<4>(param)),
      out(out),
      in(in),
      Out_(std::get<0>(param)),
      In_(std::get<1>(param)),
      outNorm(std::get<2>(param)),
      inNorm(std::get<3>(param))
    {
      strcat(aux, out.AuxString());
      if (out.GammaBasis()==in.GammaBasis()) strcat(aux, ",PreserveBasis");
      else if (out.GammaBasis() == QUDA_UKQCD_GAMMA_BASIS && in.GammaBasis() == QUDA_DEGRAND_ROSSI_GAMMA_BASIS) strcat(aux, ",NonRelBasis");
      else if (in.GammaBasis() == QUDA_UKQCD_GAMMA_BASIS && out.GammaBasis() == QUDA_DEGRAND_ROSSI_GAMMA_BASIS) strcat(aux, ",RelBasis");
      else if (out.GammaBasis() == QUDA_UKQCD_GAMMA_BASIS && in.GammaBasis() == QUDA_CHIRAL_GAMMA_BASIS) strcat(aux, ",ChiralToNonRelBasis");
      else if (in.GammaBasis() == QUDA_UKQCD_GAMMA_BASIS && out.GammaBasis() == QUDA_CHIRAL_GAMMA_BASIS) strcat(aux, ",NonRelToChiralBasis");
      else errorQuda("Basis change from %d to %d not supported", in.GammaBasis(), out.GammaBasis());

      apply(device::get_default_stream());
    }

    template <int nSpin>
    typename std::enable_if<nSpin != 4, void>::type Launch(TuneParam &tp, const qudaStream_t &stream)
    {
      constexpr bool enable_host = true;
      if (out.GammaBasis()==in.GammaBasis()) {
        launch<CopyColorSpinor_, enable_host>(tp, stream, Arg<PreserveBasis>(out, in, Out_, In_, outNorm, inNorm));
      } else {
        errorQuda("Unexpected basis change from %d to %d", in.GammaBasis(), out.GammaBasis());
      }
    }

    template <int nSpin>
    typename std::enable_if<nSpin == 4, void>::type Launch(TuneParam &tp, const qudaStream_t &stream)
    {
      constexpr bool enable_host = true;
      if (out.GammaBasis()==in.GammaBasis()) {
        launch<CopyColorSpinor_, enable_host>(tp, stream, Arg<PreserveBasis>(out, in, Out_, In_, outNorm, inNorm));
      } else if (out.GammaBasis() == QUDA_UKQCD_GAMMA_BASIS && in.GammaBasis() == QUDA_DEGRAND_ROSSI_GAMMA_BASIS) {
        launch<CopyColorSpinor_, enable_host>(tp, stream, Arg<NonRelBasis>(out, in, Out_, In_, outNorm, inNorm));
      } else if (in.GammaBasis() == QUDA_UKQCD_GAMMA_BASIS && out.GammaBasis() == QUDA_DEGRAND_ROSSI_GAMMA_BASIS) {
        launch<CopyColorSpinor_, enable_host>(tp, stream, Arg<RelBasis>(out, in, Out_, In_, outNorm, inNorm));
      } else if (out.GammaBasis() == QUDA_UKQCD_GAMMA_BASIS && in.GammaBasis() == QUDA_CHIRAL_GAMMA_BASIS) {
        launch<CopyColorSpinor_, enable_host>(tp, stream, Arg<ChiralToNonRelBasis>(out, in, Out_, In_, outNorm, inNorm));
      } else if (in.GammaBasis() == QUDA_UKQCD_GAMMA_BASIS && out.GammaBasis() == QUDA_CHIRAL_GAMMA_BASIS) {
        launch<CopyColorSpinor_, enable_host>(tp, stream, Arg<NonRelToChiralBasis>(out, in, Out_, In_, outNorm, inNorm));
      } else {
        errorQuda("Unexpected basis change from %d to %d", in.GammaBasis(), out.GammaBasis());
      }
    }

    void apply(const qudaStream_t &stream)
    {
      TuneParam tp = tuneLaunch(*this, getTuning(), getVerbosity());
      Launch<Ns>(tp, stream);
    }

    long long flops() const { return 0; }
    long long bytes() const { return in.Bytes() + out.Bytes(); }
  };

  /** Decide on the output order*/
  template <int Ns, int Nc, typename I, typename param_t>
  void genericCopyColorSpinor(ColorSpinorField &out, const ColorSpinorField &in, param_t &param)
  {
    using FloatOut = typename std::remove_pointer<typename std::tuple_element<0, param_t>::type>::type;
    if (out.isNative()) {
      using O = typename colorspinor_mapper<FloatOut,Ns,Nc>::type;
      CopyColorSpinor<Ns, Nc, O, I, param_t>(out, in, param);
    } else if (out.FieldOrder() == QUDA_FLOAT2_FIELD_ORDER && Ns == 4) {
      // this is needed for single-precision mg for changing basis in the transfer
      using O = typename colorspinor::FloatNOrder<FloatOut, 4, Nc, 2>;
      CopyColorSpinor<4, Nc, O, I, param_t>(out, in, param);
    } else if (out.FieldOrder() == QUDA_SPACE_SPIN_COLOR_FIELD_ORDER) {
      using O = SpaceSpinorColorOrder<FloatOut, Ns, Nc>;
      CopyColorSpinor<Ns, Nc, O, I, param_t>(out, in, param);
    } else if (out.FieldOrder() == QUDA_SPACE_COLOR_SPIN_FIELD_ORDER) {
      using O = SpaceColorSpinorOrder<FloatOut, Ns, Nc>;
      CopyColorSpinor<Ns, Nc, O, I, param_t>(out, in, param);
    } else if (out.FieldOrder() == QUDA_PADDED_SPACE_SPIN_COLOR_FIELD_ORDER) {

#ifdef BUILD_TIFR_INTERFACE
      using O = PaddedSpaceSpinorColorOrder<FloatOut, Ns, Nc>;
      CopyColorSpinor<Ns, Nc, O, I, param_t>(out, in, param);
#else
      errorQuda("TIFR interface has not been built\n");
#endif

    } else if (out.FieldOrder() == QUDA_QDPJIT_FIELD_ORDER) {

#ifdef BUILD_QDPJIT_INTERFACE
      using O = QDPJITDiracOrder<FloatOut, Ns, Nc>;
      CopyColorSpinor<Ns, Nc, O, I, param_t>(out, in, param);
#else
      errorQuda("QDPJIT interface has not been built\n");
#endif
    } else {
      errorQuda("Order %d not defined (Ns=%d, Nc=%d)", out.FieldOrder(), Ns, Nc);
    }
  }

  /** Decide on the input order*/
  template <int Ns, int Nc, typename param_t>
  void genericCopyColorSpinor(ColorSpinorField &out, const ColorSpinorField &in, param_t &param)
  {
    using FloatIn = typename std::remove_pointer<typename std::tuple_element<1, param_t>::type>::type;
    if (in.isNative()) {
      using I = typename colorspinor_mapper<FloatIn,Ns,Nc>::type;
      genericCopyColorSpinor<Ns, Nc, I>(out, in, param);
    } else if (in.FieldOrder() == QUDA_FLOAT2_FIELD_ORDER && Ns == 4) {
      // this is needed for single-precision mg for changing basis in the transfer
      using I = typename colorspinor::FloatNOrder<FloatIn, 4, Nc, 2>;
      genericCopyColorSpinor<4, Nc, I>(out, in, param);
    } else if (in.FieldOrder() == QUDA_SPACE_SPIN_COLOR_FIELD_ORDER) {
      using I = SpaceSpinorColorOrder<FloatIn, Ns, Nc>;
      genericCopyColorSpinor<Ns, Nc, I>(out, in, param);
    } else if (in.FieldOrder() == QUDA_SPACE_COLOR_SPIN_FIELD_ORDER) {
      using I = SpaceColorSpinorOrder<FloatIn, Ns, Nc>;
      genericCopyColorSpinor<Ns, Nc, I>(out, in, param);
    } else if (in.FieldOrder() == QUDA_PADDED_SPACE_SPIN_COLOR_FIELD_ORDER) {

#ifdef BUILD_TIFR_INTERFACE
      using ColorSpinor = PaddedSpaceSpinorColorOrder<FloatIn, Ns, Nc>;
      genericCopyColorSpinor<Ns, Nc, ColorSpinor>(out, in, param);
#else
      errorQuda("TIFR interface has not been built\n");
#endif

    } else if (in.FieldOrder() == QUDA_QDPJIT_FIELD_ORDER) {

#ifdef BUILD_QDPJIT_INTERFACE
      using ColorSpinor = QDPJITDiracOrder<FloatIn, Ns, Nc>;
      genericCopyColorSpinor<Ns, Nc, ColorSpinor>(out, in, param);
#else
      errorQuda("QDPJIT interface has not been built\n");
#endif
    } else {
      errorQuda("Order %d not defined (Ns=%d, Nc=%d)", in.FieldOrder(), Ns, Nc);
    }
  }

  template <int Ns, int Nc, typename param_t>
  void copyGenericColorSpinor(ColorSpinorField &dst, const ColorSpinorField &src, param_t &param)
  {
    if (dst.Ndim() != src.Ndim())
      errorQuda("Number of dimensions %d %d don't match", dst.Ndim(), src.Ndim());

    if (dst.Volume() != src.Volume()) errorQuda("Volumes %lu %lu don't match", dst.Volume(), src.Volume());

    if (!( dst.SiteOrder() == src.SiteOrder() ||
	   (dst.SiteOrder() == QUDA_EVEN_ODD_SITE_ORDER &&
	    src.SiteOrder() == QUDA_ODD_EVEN_SITE_ORDER) ||
	   (dst.SiteOrder() == QUDA_ODD_EVEN_SITE_ORDER &&
	    src.SiteOrder() == QUDA_EVEN_ODD_SITE_ORDER) ) ) {
      errorQuda("Subset orders %d %d don't match", dst.SiteOrder(), src.SiteOrder());
    }

    if (dst.SiteSubset() != src.SiteSubset())
      errorQuda("Subset types do not match %d %d", dst.SiteSubset(), src.SiteSubset());

    // We currently only support parity-ordered fields; even-odd or odd-even
    if (dst.SiteOrder() == QUDA_LEXICOGRAPHIC_SITE_ORDER) {
      errorQuda("Copying to full fields with lexicographical ordering is not currently supported");
    }

    if (dst.SiteSubset() == QUDA_FULL_SITE_SUBSET && (src.FieldOrder() == QUDA_QDPJIT_FIELD_ORDER || dst.FieldOrder() == QUDA_QDPJIT_FIELD_ORDER)) {
      errorQuda("QDPJIT field ordering not supported for full site fields");
    }

    genericCopyColorSpinor<Ns, Nc>(dst, src, param);
  }

  template <int Nc, typename dstFloat, typename srcFloat>
  void CopyGenericColorSpinor(ColorSpinorField &dst, const ColorSpinorField &src,
			      QudaFieldLocation location, dstFloat *Dst, srcFloat *Src,
			      float *dstNorm=0, float *srcNorm=0)
  {
    std::tuple<dstFloat*, srcFloat*, float*, float*, QudaFieldLocation> param(Dst, Src, dstNorm, srcNorm, location);

    if (dst.Nspin() != src.Nspin()) errorQuda("source and destination spins must match");

    if (dst.Nspin() == 4) {
#if defined(NSPIN4)
      copyGenericColorSpinor<4,Nc>(dst, src, param);
#else
      errorQuda("%s has not been built for Nspin=%d fields", __func__, src.Nspin());
#endif
    } else if (dst.Nspin() == 2) {
#if defined(NSPIN2)
      copyGenericColorSpinor<2,Nc>(dst, src, param);
#else
      errorQuda("%s has not been built for Nspin=%d fields", __func__, src.Nspin());
#endif
    } else if (dst.Nspin() == 1) {
#if defined(NSPIN1)
      copyGenericColorSpinor<1,Nc>(dst, src, param);
#else
      errorQuda("%s has not been built for Nspin=%d fields", __func__, src.Nspin());
#endif
    } else {
      errorQuda("Nspin=%d unsupported", dst.Nspin());
    }
  }

} // namespace quda
