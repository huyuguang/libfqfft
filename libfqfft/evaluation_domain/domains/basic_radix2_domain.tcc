/** @file
 *****************************************************************************

 Implementation of interfaces for the "basic radix-2" evaluation domain.

 See basic_radix2_domain.hpp .

 *****************************************************************************
 * @author     This file is part of libfqfft, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef BASIC_RADIX2_DOMAIN_TCC_
#define BASIC_RADIX2_DOMAIN_TCC_

#include <libff/algebra/fields/field_utils.hpp>
#include <libff/common/double.hpp>
#include <libff/common/utils.hpp>

#include <libfqfft/evaluation_domain/domains/basic_radix2_domain_aux.hpp>

namespace libfqfft {

template<typename FieldT>
basic_radix2_domain<FieldT>::basic_radix2_domain(const size_t m)
    : evaluation_domain<FieldT>(m) {
  if (m <= 1) throw InvalidSizeException("basic_radix2(): expected m > 1");

  if (!std::is_same<FieldT, libff::Double>::value) {
    const size_t logm = libff::log2(m);
    if (logm > (FieldT::s))
      throw DomainSizeException("basic_radix2(): expected logm <= FieldT::s");
  }

  // try { omega = libff::get_root_of_unity<FieldT>(m); }
  // catch (const std::invalid_argument& e) { throw
  // DomainSizeException(e.what()); }
  bool success;
  omega = libff::get_root_of_unity2<FieldT>(m, &success);
  if (!success) throw DomainSizeException("libff::get_root_of_unity2 invalid argument");
}

template<typename FieldT>
std::shared_ptr<basic_radix2_domain<FieldT>> basic_radix2_domain<FieldT>::create_ptr(const size_t m)
{
  std::shared_ptr<basic_radix2_domain<FieldT>> result;
  if (m <= 1) return result;

  if (!std::is_same<FieldT, libff::Double>::value) {
    const size_t logm = libff::log2(m);
    if (logm > (FieldT::s)) return result;
  }

  bool success;
  libff::get_root_of_unity2<FieldT>(m, &success);
  if (!success) return result;

  result.reset(new basic_radix2_domain<FieldT>(m));
  return result;
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::FFT(std::vector<FieldT> &a)
{
    if (a.size() != this->m) throw DomainSizeException("basic_radix2: expected a.size() == this->m");

    _basic_radix2_FFT(a, omega);
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::iFFT(std::vector<FieldT> &a)
{
    if (a.size() != this->m) throw DomainSizeException("basic_radix2: expected a.size() == this->m");

    _basic_radix2_FFT(a, omega.inverse());

    const FieldT sconst = FieldT(a.size()).inverse();
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] *= sconst;
    }
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::cosetFFT(std::vector<FieldT> &a, const FieldT &g)
{
    _multiply_by_coset(a, g);
    FFT(a);
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::icosetFFT(std::vector<FieldT> &a, const FieldT &g)
{
    iFFT(a);
    _multiply_by_coset(a, g.inverse());
}

template<typename FieldT>
std::vector<FieldT> basic_radix2_domain<FieldT>::evaluate_all_lagrange_polynomials(const FieldT &t)
{
    return _basic_radix2_evaluate_all_lagrange_polynomials(this->m, t);
}

template<typename FieldT>
FieldT basic_radix2_domain<FieldT>::get_domain_element(const size_t idx)
{
    return omega^idx;
}

template<typename FieldT>
FieldT basic_radix2_domain<FieldT>::compute_vanishing_polynomial(const FieldT &t)
{
    return (t^this->m) - FieldT::one();
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::add_poly_Z(const FieldT &coeff, std::vector<FieldT> &H)
{
    libff::enter_block("basic_radix2_domain::add_poly_Z");

    if (H.size() != this->m+1) throw DomainSizeException("basic_radix2: expected H.size() == this->m+1");

    H[this->m] += coeff;
    H[0] -= coeff;

    libff::leave_block("basic_radix2_domain::add_poly_Z");
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::divide_by_Z_on_coset(std::vector<FieldT> &P)
{
    const FieldT coset = FieldT::multiplicative_generator;
    const FieldT Z_inverse_at_coset = this->compute_vanishing_polynomial(coset).inverse();
    for (size_t i = 0; i < this->m; ++i)
    {
        P[i] *= Z_inverse_at_coset;
    }
}

} // libfqfft

#endif // BASIC_RADIX2_DOMAIN_TCC_
