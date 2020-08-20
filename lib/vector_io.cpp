#include <color_spinor_field.h>
#include <qio_field.h>
#include <vector_io.h>
#include <blas_quda.h>

namespace quda
{

  VectorIO::VectorIO(const std::string &filename, bool parity_inflate) :
#ifdef HAVE_QIO
    filename(filename),
    parity_inflate(parity_inflate)
#else
    filename(filename)
#endif
  {
    if (strcmp(filename.c_str(), "") == 0) { errorQuda("No eigenspace input file defined."); }
  }

  void VectorIO::load(std::vector<ColorSpinorField *> &vecs)
  {
#ifdef HAVE_QIO
    const int Nvec = vecs.size();
    auto spinor_parity = vecs[0]->SuggestedParity();
    if (getVerbosity() >= QUDA_SUMMARIZE) printfQuda("Start loading %04d vectors from %s\n", Nvec, filename.c_str());

    std::vector<ColorSpinorField *> tmp;
    tmp.reserve(Nvec);
    if (vecs[0]->Location() == QUDA_CUDA_FIELD_LOCATION) {
      ColorSpinorParam csParam(*vecs[0]);
      csParam.fieldOrder = QUDA_SPACE_SPIN_COLOR_FIELD_ORDER;
      csParam.setPrecision(vecs[0]->Precision() < QUDA_SINGLE_PRECISION ? QUDA_SINGLE_PRECISION : vecs[0]->Precision());
      csParam.location = QUDA_CPU_FIELD_LOCATION;
      csParam.create = QUDA_NULL_FIELD_CREATE;
      if (csParam.siteSubset == QUDA_PARITY_SITE_SUBSET && parity_inflate) {
        csParam.x[0] *= 2;
        csParam.siteSubset = QUDA_FULL_SITE_SUBSET;
      }
      for (int i = 0; i < Nvec; i++) { tmp.push_back(ColorSpinorField::Create(csParam)); }
    } else {
      ColorSpinorParam csParam(*vecs[0]);
      if (csParam.siteSubset == QUDA_PARITY_SITE_SUBSET && parity_inflate) {
        csParam.x[0] *= 2;
        csParam.siteSubset = QUDA_FULL_SITE_SUBSET;
        for (int i = 0; i < Nvec; i++) { tmp.push_back(ColorSpinorField::Create(csParam)); }
      } else {
        for (int i = 0; i < Nvec; i++) { tmp.push_back(vecs[i]); }
      }
    }

    if (vecs[0]->Ndim() == 4 || vecs[0]->Ndim() == 5) {
      // since QIO routines presently assume we have 4-d fields, we need to convert to array of 4-d fields
      auto Ls = vecs[0]->Ndim() == 5 ? tmp[0]->X(4) : 1;
      auto V4 = tmp[0]->Volume() / Ls;
      auto stride = V4 * tmp[0]->Ncolor() * tmp[0]->Nspin() * 2 * tmp[0]->Precision();
      void **V = static_cast<void **>(safe_malloc(Nvec * Ls * sizeof(void *)));
      for (int i = 0; i < Nvec; i++) {
        for (int j = 0; j < Ls; j++) { V[i * Ls + j] = static_cast<char *>(tmp[i]->V()) + j * stride; }
      }

      read_spinor_field(filename.c_str(), &V[0], tmp[0]->Precision(), tmp[0]->X(), tmp[0]->SiteSubset(), spinor_parity,
                        tmp[0]->Ncolor(), tmp[0]->Nspin(), Nvec * Ls, 0, (char **)0);

      host_free(V);
    } else {
      errorQuda("Unexpected field dimension %d", vecs[0]->Ndim());
    }

    if (vecs[0]->Location() == QUDA_CUDA_FIELD_LOCATION) {

      ColorSpinorParam csParam(*vecs[0]);
      if (csParam.siteSubset == QUDA_FULL_SITE_SUBSET || !parity_inflate) {
        for (int i = 0; i < Nvec; i++) {
          *vecs[i] = *tmp[i];
          delete tmp[i];
        }
      } else {
        // Create a temporary single-parity CPU field
        csParam.fieldOrder = QUDA_SPACE_SPIN_COLOR_FIELD_ORDER;
        csParam.setPrecision(vecs[0]->Precision() < QUDA_SINGLE_PRECISION ? QUDA_SINGLE_PRECISION : vecs[0]->Precision());
        csParam.location = QUDA_CPU_FIELD_LOCATION;
        csParam.create = QUDA_NULL_FIELD_CREATE;

        ColorSpinorField *tmp_intermediate = ColorSpinorField::Create(csParam);

        for (int i = 0; i < Nvec; i++) {
          if (spinor_parity == QUDA_EVEN_PARITY)
            blas::copy(*tmp_intermediate, tmp[i]->Even());
          else if (spinor_parity == QUDA_ODD_PARITY)
            blas::copy(*tmp_intermediate, tmp[i]->Odd());
          else
            errorQuda("When loading single parity vectors, the suggested parity must be set.");

          *vecs[i] = *tmp_intermediate;
          delete tmp[i];
        }

        delete tmp_intermediate;
      }
    } else if (vecs[0]->Location() == QUDA_CPU_FIELD_LOCATION && vecs[0]->SiteSubset() == QUDA_PARITY_SITE_SUBSET) {
      for (int i = 0; i < Nvec; i++) {
        if (spinor_parity == QUDA_EVEN_PARITY)
          blas::copy(*vecs[i], tmp[i]->Even());
        else if (spinor_parity == QUDA_ODD_PARITY)
          blas::copy(*vecs[i], tmp[i]->Odd());
        else
          errorQuda("When loading single parity vectors, the suggested parity must be set.");

        delete tmp[i];
      }
    }

    if (getVerbosity() >= QUDA_SUMMARIZE) printfQuda("Done loading vectors\n");
#else
    errorQuda("\nQIO library was not built.\n");
#endif
  }

  void VectorIO::loadProp(std::vector<ColorSpinorField *> &vecs)
  {
#ifdef HAVE_QIO
    if (vecs.size() != 12) errorQuda("Must have 12 vectors in propagator, passed %lu", vecs.size());
    const int Nvec = vecs.size();
    auto spinor_parity = vecs[0]->SuggestedParity();
    if (getVerbosity() >= QUDA_SUMMARIZE) printfQuda("Start loading %04d vectors from %s\n", Nvec, filename.c_str());

    std::vector<ColorSpinorField *> tmp;
    tmp.reserve(Nvec);
    if (vecs[0]->Location() == QUDA_CUDA_FIELD_LOCATION) {
      ColorSpinorParam csParam(*vecs[0]);
      csParam.fieldOrder = QUDA_SPACE_SPIN_COLOR_FIELD_ORDER;
      csParam.setPrecision(vecs[0]->Precision() < QUDA_SINGLE_PRECISION ? QUDA_SINGLE_PRECISION : vecs[0]->Precision());
      csParam.location = QUDA_CPU_FIELD_LOCATION;
      csParam.create = QUDA_NULL_FIELD_CREATE;
      if (csParam.siteSubset == QUDA_PARITY_SITE_SUBSET && parity_inflate) {
        csParam.x[0] *= 2;
        csParam.siteSubset = QUDA_FULL_SITE_SUBSET;
      }
      for (int i = 0; i < Nvec; i++) { tmp.push_back(ColorSpinorField::Create(csParam)); }
    } else {
      ColorSpinorParam csParam(*vecs[0]);
      if (csParam.siteSubset == QUDA_PARITY_SITE_SUBSET && parity_inflate) {
        csParam.x[0] *= 2;
        csParam.siteSubset = QUDA_FULL_SITE_SUBSET;
        for (int i = 0; i < Nvec; i++) { tmp.push_back(ColorSpinorField::Create(csParam)); }
      } else {
        for (int i = 0; i < Nvec; i++) { tmp.push_back(vecs[i]); }
      }
    }

    if (vecs[0]->Ndim() == 4 || vecs[0]->Ndim() == 5) {
      // since QIO routines presently assume we have 4-d fields, we need to convert to array of 4-d fields
      auto Ls = vecs[0]->Ndim() == 5 ? tmp[0]->X(4) : 1;
      auto V4 = tmp[0]->Volume() / Ls;
      auto stride = V4 * tmp[0]->Ncolor() * tmp[0]->Nspin() * 2 * tmp[0]->Precision();
      void **V = static_cast<void **>(safe_malloc(Nvec * Ls * sizeof(void *)));
      for (int i = 0; i < Nvec; i++) {
        for (int j = 0; j < Ls; j++) { V[i * Ls + j] = static_cast<char *>(tmp[i]->V()) + j * stride; }
      }

      read_propagator_field(filename.c_str(), &V[0], tmp[0]->Precision(), tmp[0]->X(), tmp[0]->SiteSubset(),
                            spinor_parity, tmp[0]->Ncolor(), tmp[0]->Nspin(), Nvec / 12, 0, (char **)0);

      host_free(V);
    } else {
      errorQuda("Unexpected field dimension %d", vecs[0]->Ndim());
    }

    if (vecs[0]->Location() == QUDA_CUDA_FIELD_LOCATION) {

      ColorSpinorParam csParam(*vecs[0]);
      if (csParam.siteSubset == QUDA_FULL_SITE_SUBSET || !parity_inflate) {
        for (int i = 0; i < Nvec; i++) {
          *vecs[i] = *tmp[i];
          delete tmp[i];
        }
      } else {
        // Create a temporary single-parity CPU field
        csParam.fieldOrder = QUDA_SPACE_SPIN_COLOR_FIELD_ORDER;
        csParam.setPrecision(vecs[0]->Precision() < QUDA_SINGLE_PRECISION ? QUDA_SINGLE_PRECISION : vecs[0]->Precision());
        csParam.location = QUDA_CPU_FIELD_LOCATION;
        csParam.create = QUDA_NULL_FIELD_CREATE;

        ColorSpinorField *tmp_intermediate = ColorSpinorField::Create(csParam);

        for (int i = 0; i < Nvec; i++) {
          if (spinor_parity == QUDA_EVEN_PARITY)
            blas::copy(*tmp_intermediate, tmp[i]->Even());
          else if (spinor_parity == QUDA_ODD_PARITY)
            blas::copy(*tmp_intermediate, tmp[i]->Odd());
          else
            errorQuda("When loading single parity vectors, the suggested parity must be set.");

          *vecs[i] = *tmp_intermediate;
          delete tmp[i];
        }

        delete tmp_intermediate;
      }
    } else if (vecs[0]->Location() == QUDA_CPU_FIELD_LOCATION && vecs[0]->SiteSubset() == QUDA_PARITY_SITE_SUBSET) {
      for (int i = 0; i < Nvec; i++) {
        if (spinor_parity == QUDA_EVEN_PARITY)
          blas::copy(*vecs[i], tmp[i]->Even());
        else if (spinor_parity == QUDA_ODD_PARITY)
          blas::copy(*vecs[i], tmp[i]->Odd());
        else
          errorQuda("When loading single parity vectors, the suggested parity must be set.");

        delete tmp[i];
      }
    }

    if (getVerbosity() >= QUDA_SUMMARIZE) printfQuda("Done loading vectors\n");
#else
    errorQuda("\nQIO library was not built.\n");
#endif
  }

  void VectorIO::save(const std::vector<ColorSpinorField *> &vecs)
  {
#ifdef HAVE_QIO
    const int Nvec = vecs.size();
    std::vector<ColorSpinorField *> tmp;
    tmp.reserve(Nvec);
    auto spinor_parity = vecs[0]->SuggestedParity();
    if (vecs[0]->Location() == QUDA_CUDA_FIELD_LOCATION) {
      ColorSpinorParam csParam(*vecs[0]);
      csParam.fieldOrder = QUDA_SPACE_SPIN_COLOR_FIELD_ORDER;
      csParam.setPrecision(vecs[0]->Precision() < QUDA_SINGLE_PRECISION ? QUDA_SINGLE_PRECISION : vecs[0]->Precision());
      csParam.location = QUDA_CPU_FIELD_LOCATION;

      if (csParam.siteSubset == QUDA_FULL_SITE_SUBSET || !parity_inflate) {
        // We're good, copy as is.
        csParam.create = QUDA_NULL_FIELD_CREATE;
        for (int i = 0; i < Nvec; i++) {
          tmp.push_back(ColorSpinorField::Create(csParam));
          *tmp[i] = *vecs[i];
        }
      } else { // QUDA_PARITY_SITE_SUBSET
        csParam.create = QUDA_NULL_FIELD_CREATE;

        // intermediate host single-parity field
        ColorSpinorField *tmp_intermediate = ColorSpinorField::Create(csParam);

        csParam.x[0] *= 2;                          // corrects for the factor of two in the X direction
        csParam.siteSubset = QUDA_FULL_SITE_SUBSET; // create a full-parity field.
        csParam.create = QUDA_ZERO_FIELD_CREATE;    // to explicitly zero the odd sites.
        for (int i = 0; i < Nvec; i++) {
          tmp.push_back(ColorSpinorField::Create(csParam));

          // copy the single parity eigen/singular vector into an
          // intermediate device-side vector
          *tmp_intermediate = *vecs[i];

          // copy the single parity only eigen/singular vector into the even components of the full parity vector
          if (spinor_parity == QUDA_EVEN_PARITY)
            blas::copy(tmp[i]->Even(), *tmp_intermediate);
          else if (spinor_parity == QUDA_ODD_PARITY)
            blas::copy(tmp[i]->Odd(), *tmp_intermediate);
          else
            errorQuda("When saving single parity vectors, the suggested parity must be set.");
        }
        delete tmp_intermediate;
      }
    } else {
      ColorSpinorParam csParam(*vecs[0]);
      if (csParam.siteSubset == QUDA_PARITY_SITE_SUBSET && parity_inflate) {
        csParam.x[0] *= 2;
        csParam.siteSubset = QUDA_FULL_SITE_SUBSET;
        csParam.create = QUDA_ZERO_FIELD_CREATE;
        for (int i = 0; i < Nvec; i++) {
          tmp.push_back(ColorSpinorField::Create(csParam));
          if (spinor_parity == QUDA_EVEN_PARITY)
            blas::copy(tmp[i]->Even(), *vecs[i]);
          else if (spinor_parity == QUDA_ODD_PARITY)
            blas::copy(tmp[i]->Odd(), *vecs[i]);
          else
            errorQuda("When saving single parity vectors, the suggested parity must be set.");
        }
      } else {
        for (int i = 0; i < Nvec; i++) { tmp.push_back(vecs[i]); }
      }
    }

    if (getVerbosity() >= QUDA_SUMMARIZE) printfQuda("Start saving %d vectors to %s\n", Nvec, filename.c_str());

    if (vecs[0]->Ndim() == 4 || vecs[0]->Ndim() == 5) {
      // since QIO routines presently assume we have 4-d fields, we need to convert to array of 4-d fields
      auto Ls = vecs[0]->Ndim() == 5 ? tmp[0]->X(4) : 1;
      auto V4 = tmp[0]->Volume() / Ls;
      auto stride = V4 * tmp[0]->Ncolor() * tmp[0]->Nspin() * 2 * tmp[0]->Precision();
      void **V = static_cast<void **>(safe_malloc(Nvec * Ls * sizeof(void *)));
      for (int i = 0; i < Nvec; i++) {
        for (int j = 0; j < Ls; j++) { V[i * Ls + j] = static_cast<char *>(tmp[i]->V()) + j * stride; }
      }

      write_spinor_field(filename.c_str(), &V[0], tmp[0]->Precision(), tmp[0]->X(), tmp[0]->SiteSubset(), spinor_parity,
                         tmp[0]->Ncolor(), tmp[0]->Nspin(), Nvec * Ls, 0, (char **)0);

      host_free(V);
    } else {
      errorQuda("Unexpected field dimension %d", vecs[0]->Ndim());
    }

    if (getVerbosity() >= QUDA_SUMMARIZE) printfQuda("Done saving vectors\n");
    if (vecs[0]->Location() == QUDA_CUDA_FIELD_LOCATION
        || (vecs[0]->Location() == QUDA_CPU_FIELD_LOCATION && vecs[0]->SiteSubset() == QUDA_PARITY_SITE_SUBSET)) {
      for (int i = 0; i < Nvec; i++) delete tmp[i];
    }
#else
    errorQuda("\nQIO library was not built.\n");
#endif
  }

  void VectorIO::saveProp(const std::vector<ColorSpinorField *> &vecs)
  {
#ifdef HAVE_QIO
    if (vecs.size() != 12) errorQuda("Must have 12 vectors in propagator, passed %lu", vecs.size());

    const int Nvec = vecs.size();
    std::vector<ColorSpinorField *> tmp;
    tmp.reserve(Nvec);
    auto spinor_parity = vecs[0]->SuggestedParity();
    if (vecs[0]->Location() == QUDA_CUDA_FIELD_LOCATION) {
      ColorSpinorParam csParam(*vecs[0]);
      csParam.fieldOrder = QUDA_SPACE_SPIN_COLOR_FIELD_ORDER;
      csParam.setPrecision(vecs[0]->Precision() < QUDA_SINGLE_PRECISION ? QUDA_SINGLE_PRECISION : vecs[0]->Precision());
      csParam.location = QUDA_CPU_FIELD_LOCATION;

      if (csParam.siteSubset == QUDA_FULL_SITE_SUBSET || !parity_inflate) {
        // We're good, copy as is.
        csParam.create = QUDA_NULL_FIELD_CREATE;
        for (int i = 0; i < Nvec; i++) {
          tmp.push_back(ColorSpinorField::Create(csParam));
          *tmp[i] = *vecs[i];
        }
      } else { // QUDA_PARITY_SITE_SUBSET
        csParam.create = QUDA_NULL_FIELD_CREATE;

        // intermediate host single-parity field
        ColorSpinorField *tmp_intermediate = ColorSpinorField::Create(csParam);

        csParam.x[0] *= 2;                          // corrects for the factor of two in the X direction
        csParam.siteSubset = QUDA_FULL_SITE_SUBSET; // create a full-parity field.
        csParam.create = QUDA_ZERO_FIELD_CREATE;    // to explicitly zero the odd sites.
        for (int i = 0; i < Nvec; i++) {
          tmp.push_back(ColorSpinorField::Create(csParam));

          // copy the single parity eigen/singular vector into an
          // intermediate device-side vector
          *tmp_intermediate = *vecs[i];

          // copy the single parity only eigen/singular vector into the even components of the full parity vector
          if (spinor_parity == QUDA_EVEN_PARITY)
            blas::copy(tmp[i]->Even(), *tmp_intermediate);
          else if (spinor_parity == QUDA_ODD_PARITY)
            blas::copy(tmp[i]->Odd(), *tmp_intermediate);
          else
            errorQuda("When saving single parity vectors, the suggested parity must be set.");
        }
        delete tmp_intermediate;
      }
    } else {
      ColorSpinorParam csParam(*vecs[0]);
      if (csParam.siteSubset == QUDA_PARITY_SITE_SUBSET && parity_inflate) {
        csParam.x[0] *= 2;
        csParam.siteSubset = QUDA_FULL_SITE_SUBSET;
        csParam.create = QUDA_ZERO_FIELD_CREATE;
        for (int i = 0; i < Nvec; i++) {
          tmp.push_back(ColorSpinorField::Create(csParam));
          if (spinor_parity == QUDA_EVEN_PARITY)
            blas::copy(tmp[i]->Even(), *vecs[i]);
          else if (spinor_parity == QUDA_ODD_PARITY)
            blas::copy(tmp[i]->Odd(), *vecs[i]);
          else
            errorQuda("When saving single parity vectors, the suggested parity must be set.");
        }
      } else {
        for (int i = 0; i < Nvec; i++) { tmp.push_back(vecs[i]); }
      }
    }

    if (getVerbosity() >= QUDA_SUMMARIZE) printfQuda("Start saving %d vectors to %s\n", Nvec, filename.c_str());

    if (vecs[0]->Ndim() == 4 || vecs[0]->Ndim() == 5) {
      // since QIO routines presently assume we have 4-d fields, we need to convert to array of 4-d fields
      auto Ls = vecs[0]->Ndim() == 5 ? tmp[0]->X(4) : 1;
      auto V4 = tmp[0]->Volume() / Ls;
      auto stride = V4 * tmp[0]->Ncolor() * tmp[0]->Nspin() * 2 * tmp[0]->Precision();
      void **V = static_cast<void **>(safe_malloc(Nvec * Ls * sizeof(void *)));
      for (int i = 0; i < Nvec; i++) {
        for (int j = 0; j < Ls; j++) { V[i * Ls + j] = static_cast<char *>(tmp[i]->V()) + j * stride; }
      }

      write_propagator_field(filename.c_str(), &V[0], tmp[0]->Precision(), tmp[0]->X(), tmp[0]->SiteSubset(),
                             spinor_parity, tmp[0]->Ncolor(), tmp[0]->Nspin(), (Nvec) / 12, 0, (char **)0);

      host_free(V);
    } else {
      errorQuda("Unexpected field dimension %d", vecs[0]->Ndim());
    }

    if (getVerbosity() >= QUDA_SUMMARIZE) printfQuda("Done saving vectors\n");
    if (vecs[0]->Location() == QUDA_CUDA_FIELD_LOCATION
        || (vecs[0]->Location() == QUDA_CPU_FIELD_LOCATION && vecs[0]->SiteSubset() == QUDA_PARITY_SITE_SUBSET)) {
      for (int i = 0; i < Nvec; i++) delete tmp[i];
    }
#else
    errorQuda("\nQIO library was not built.\n");
#endif
  }

  void VectorIO::downPrec(const std::vector<ColorSpinorField *> &vecs_high_prec,
                          std::vector<ColorSpinorField *> &vecs_low_prec, const QudaPrecision low_prec)
  {
    if (low_prec >= vecs_high_prec[0]->Precision()) {
      errorQuda("Attempting to down-prec from precision %d to %d", vecs_high_prec[0]->Precision(), low_prec);
    }

    ColorSpinorParam csParamClone(*vecs_high_prec[0]);
    csParamClone.create = QUDA_REFERENCE_FIELD_CREATE;
    csParamClone.setPrecision(low_prec);
    for (unsigned int i = 0; i < vecs_high_prec.size(); i++) {
      vecs_low_prec.push_back(vecs_high_prec[i]->CreateAlias(csParamClone));
    }
    if (getVerbosity() >= QUDA_SUMMARIZE) {
      printfQuda("Vector space successfully down copied from prec %d to prec %d\n", vecs_high_prec[0]->Precision(),
                 vecs_low_prec[0]->Precision());
    }
  }

} // namespace quda
