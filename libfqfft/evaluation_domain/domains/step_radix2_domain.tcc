/** @file
 *****************************************************************************

 Implementation of interfaces for the "step radix-2" evaluation domain.

 See step_radix2_domain.hpp .

 *****************************************************************************
 * @author     This file is part of libfqfft, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef STEP_RADIX2_DOMAIN_TCC_

#include <libfqfft/evaluation_domain/domains/basic_radix2_domain_aux.hpp>

namespace libfqfft {

template<typename FieldT>
step_radix2_domain<FieldT>::step_radix2_domain(const size_t m) : evaluation_domain<FieldT>(m)
{
    if (m <= 1) throw InvalidSizeException("step_radix2(): expected m > 1");

    big_m = ((size_t)1) <<(libff::log2(m)-1);
    small_m = m - big_m;

    if (small_m != ((size_t)1) << libff::log2(small_m)) {
      std::cerr << "step_radix2(): expected small_m == ((size_t)1)<<libff::log2(small_m): " << small_m << "\n";
      throw DomainSizeException("step_radix2(): expected small_m == ((size_t)1)<<libff::log2(small_m)");
    }        

    //try { omega = libff::get_root_of_unity<FieldT>((size_t)1ul<<libff::log2(m)); }
    //catch (const std::invalid_argument& e) { throw DomainSizeException(e.what()); }
    bool success;
    omega = libff::get_root_of_unity2<FieldT>(((size_t)1)<<libff::log2(m), &success);
    if (!success) {
      std::cerr << "get_root_of_unity2 failed: " << ((size_t)1)<<libff::log2(m) << "\n";
      throw DomainSizeException("libff::get_root_of_unity invalid argument (1)");
    }

    big_omega = omega.squared();
    small_omega = libff::get_root_of_unity2<FieldT>(small_m, &success);
    if (!success) {
      std::cerr << "get_root_of_unity2 failed: " << small_m << "," << small_omega << "\n";
      throw std::invalid_argument("libff::get_root_of_unity invalid argument (2)");
    }
}

template<typename FieldT>
std::shared_ptr<step_radix2_domain<FieldT>> step_radix2_domain<FieldT>::create_ptr(const size_t m)
{
  std::shared_ptr<step_radix2_domain<FieldT>> result;
  if (m <= 1) return result;

  auto big_m = ((size_t)1)<<(libff::log2(m)-1);
  auto small_m = m - big_m;

  if (small_m != ((size_t)1) << libff::log2(small_m)) return result;

  bool success;
  auto omega =
      libff::get_root_of_unity2<FieldT>(((size_t)1) << libff::log2(m), &success);
  if (!success) return result;

  auto big_omega = omega.squared();
  auto small_omega = libff::get_root_of_unity2<FieldT>(small_m, &success);
  if (!success) return result;

  result.reset(new step_radix2_domain<FieldT>(m));
  return result;
}

template<typename FieldT>
void step_radix2_domain<FieldT>::FFT(std::vector<FieldT> &a)
{
    if (a.size() != this->m) throw DomainSizeException("step_radix2: expected a.size() == this->m");

    std::vector<FieldT> c(big_m, FieldT::zero());
    std::vector<FieldT> d(big_m, FieldT::zero());

    FieldT omega_i = FieldT::one();
    for (size_t i = 0; i < big_m; ++i)
    {
        c[i] = (i < small_m ? a[i] + a[i+big_m] : a[i]);
        d[i] = omega_i * (i < small_m ? a[i] - a[i+big_m] : a[i]);
        omega_i *= omega;
    }

    std::vector<FieldT> e(small_m, FieldT::zero());
    const size_t compr = ((size_t)1) <<(libff::log2(big_m) - libff::log2(small_m));
    for (size_t i = 0; i < small_m; ++i)
    {
        for (size_t j = 0; j < compr; ++j)
        {
            e[i] += d[i + j * small_m];
        }
    }

    _basic_radix2_FFT(c, omega.squared());
    _basic_radix2_FFT(e, libff::get_root_of_unity<FieldT>(small_m));

    for (size_t i = 0; i < big_m; ++i)
    {
        a[i] = c[i];
    }

    for (size_t i = 0; i < small_m; ++i)
    {
        a[i+big_m] = e[i];
    }
}

template<typename FieldT>
void step_radix2_domain<FieldT>::iFFT(std::vector<FieldT> &a)
{
    if (a.size() != this->m) throw DomainSizeException("step_radix2: expected a.size() == this->m");

    std::vector<FieldT> U0(a.begin(), a.begin() + big_m);
    std::vector<FieldT> U1(a.begin() + big_m, a.end());

    _basic_radix2_FFT(U0, omega.squared().inverse());
    _basic_radix2_FFT(U1, libff::get_root_of_unity<FieldT>(small_m).inverse());

    const FieldT U0_size_inv = FieldT(big_m).inverse();
    for (size_t i = 0; i < big_m; ++i)
    {
        U0[i] *= U0_size_inv;
    }

    const FieldT U1_size_inv = FieldT(small_m).inverse();
    for (size_t i = 0; i < small_m; ++i)
    {
        U1[i] *= U1_size_inv;
    }

    std::vector<FieldT> tmp = U0;
    FieldT omega_i = FieldT::one();
    for (size_t i = 0; i < big_m; ++i)
    {
        tmp[i] *= omega_i;
        omega_i *= omega;
    }

    // save A_suffix
    for (size_t i = small_m; i < big_m; ++i)
    {
        a[i] = U0[i];
    }

    const size_t compr = ((size_t)1) <<(libff::log2(big_m) - libff::log2(small_m));
    for (size_t i = 0; i < small_m; ++i)
    {
        for (size_t j = 1; j < compr; ++j)
        {
            U1[i] -= tmp[i + j * small_m];
        }
    }

    const FieldT omega_inv = omega.inverse();
    FieldT omega_inv_i = FieldT::one();
    for (size_t i = 0; i < small_m; ++i)
    {
        U1[i] *= omega_inv_i;
        omega_inv_i *= omega_inv;
    }

    // compute A_prefix
    const FieldT over_two = FieldT(2).inverse();
    for (size_t i = 0; i < small_m; ++i)
    {
        a[i] = (U0[i]+U1[i]) * over_two;
    }

    // compute B2
    for (size_t i = 0; i < small_m; ++i)
    {
        a[big_m + i] = (U0[i]-U1[i]) * over_two;
    }
}

template<typename FieldT>
void step_radix2_domain<FieldT>::cosetFFT(std::vector<FieldT> &a, const FieldT &g)
{
    _multiply_by_coset(a, g);
    FFT(a);
}

template<typename FieldT>
void step_radix2_domain<FieldT>::icosetFFT(std::vector<FieldT> &a, const FieldT &g)
{
    iFFT(a);
    _multiply_by_coset(a, g.inverse());
}

template<typename FieldT>
std::vector<FieldT> step_radix2_domain<FieldT>::evaluate_all_lagrange_polynomials(const FieldT &t)
{
    std::vector<FieldT> inner_big = _basic_radix2_evaluate_all_lagrange_polynomials(big_m, t);
    std::vector<FieldT> inner_small = _basic_radix2_evaluate_all_lagrange_polynomials(small_m, t * omega.inverse());

    std::vector<FieldT> result(this->m, FieldT::zero());

    const FieldT L0 = (t^small_m)-(omega^small_m);
    const FieldT omega_to_small_m = omega^small_m;
    const FieldT big_omega_to_small_m = big_omega ^ small_m;
    FieldT elt = FieldT::one();
    for (size_t i = 0; i < big_m; ++i)
    {
        result[i] = inner_big[i] * L0 * (elt - omega_to_small_m).inverse();
        elt *= big_omega_to_small_m;
    }

    const FieldT L1 = ((t^big_m)-FieldT::one()) * ((omega^big_m) - FieldT::one()).inverse();

    for (size_t i = 0; i < small_m; ++i)
    {
        result[big_m + i] = L1 * inner_small[i];
    }

    return result;
}

template<typename FieldT>
FieldT step_radix2_domain<FieldT>::get_domain_element(const size_t idx)
{
    if (idx < big_m)
    {
        return big_omega^idx;
    }
    else
    {
        return omega * (small_omega^(idx-big_m));
    }
}

template<typename FieldT>
FieldT step_radix2_domain<FieldT>::compute_vanishing_polynomial(const FieldT &t)
{
    return ((t^big_m) - FieldT::one()) * ((t^small_m) - (omega^small_m));
}

template<typename FieldT>
void step_radix2_domain<FieldT>::add_poly_Z(const FieldT &coeff, std::vector<FieldT> &H)
{
    libff::enter_block("step_radix2_domain::add_poly_Z");
    if (H.size() != this->m+1) throw DomainSizeException("step_radix2: expected H.size() == this->m+1");

    const FieldT omega_to_small_m = omega^small_m;

    H[this->m] += coeff;
    H[big_m] -= coeff * omega_to_small_m;
    H[small_m] -= coeff;
    H[0] += coeff * omega_to_small_m;

    libff::leave_block("step_radix2_domain::add_poly_Z");
}

template<typename FieldT>
void step_radix2_domain<FieldT>::divide_by_Z_on_coset(std::vector<FieldT> &P)
{
    // (c^{2^k}-1) * (c^{2^r} * w^{2^{r+1}*i) - w^{2^r})
    const FieldT coset = FieldT::multiplicative_generator;

    const FieldT Z0 = (coset^big_m) - FieldT::one();
    const FieldT coset_to_small_m_times_Z0 = (coset^small_m) * Z0;
    const FieldT omega_to_small_m_times_Z0 = (omega^small_m) * Z0;
    const FieldT omega_to_2small_m = omega^(2*small_m);
    FieldT elt = FieldT::one();

    for (size_t i = 0; i < big_m; ++i)
    {
        P[i] *= (coset_to_small_m_times_Z0 * elt - omega_to_small_m_times_Z0).inverse();
        elt *= omega_to_2small_m;
    }

    // (c^{2^k}*w^{2^k}-1) * (c^{2^k} * w^{2^r} - w^{2^r})

    const FieldT Z1 = ((((coset*omega)^big_m) - FieldT::one()) * (((coset * omega)^small_m) - (omega^small_m)));
    const FieldT Z1_inverse = Z1.inverse();

    for (size_t i = 0; i < small_m; ++i)
    {
        P[big_m + i] *= Z1_inverse;
    }

}

} // libfqfft

#endif // STEP_RADIX2_DOMAIN_TCC_
