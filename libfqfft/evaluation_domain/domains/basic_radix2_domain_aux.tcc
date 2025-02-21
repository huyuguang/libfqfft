/** @file
 *****************************************************************************

 Implementation of interfaces for auxiliary functions for the "basic radix-2" evaluation domain.

 See basic_radix2_domain_aux.hpp .

 *****************************************************************************
 * @author     This file is part of libfqfft, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef BASIC_RADIX2_DOMAIN_AUX_TCC_
#define BASIC_RADIX2_DOMAIN_AUX_TCC_

#include <algorithm>
#include <vector>

#ifdef MULTICORE
#include <omp.h>
#endif

#include <libff/algebra/fields/field_utils.hpp>

#include <libfqfft/tools/exceptions.hpp>

#ifdef DEBUG
#include <libff/common/profiling.hpp>
#endif

namespace libfqfft {

#ifdef MULTICORE
#define _basic_radix2_FFT _basic_parallel_radix2_FFT
#else
#define _basic_radix2_FFT _basic_serial_radix2_FFT
#endif

/*
 Below we make use of pseudocode from [CLRS 2n Ed, pp. 864].
 Also, note that it's the caller's responsibility to multiply by 1/N.
 */
template<typename FieldT>
void _basic_serial_radix2_FFT(std::vector<FieldT> &a, const FieldT &omega)
{
    const size_t n = a.size(), logn = libff::log2(n);
    if (n != ((size_t)1 << logn)) throw DomainSizeException("expected n == ((size_t)1 << logn)");

    /* swapping in place (from Storer's book) */
    for (size_t k = 0; k < n; ++k)
    {
        const size_t rk = libff::bitreverse(k, logn);
        if (k < rk)
            std::swap(a[k], a[rk]);
    }

    size_t m = 1; // invariant: m = 2^{s-1}
    for (size_t s = 1; s <= logn; ++s)
    {
        // w_m is 2^s-th root of unity now
        const FieldT w_m = omega^(n/(2*m));
#ifndef _MSC_VER
        asm volatile  ("/* pre-inner */");
#endif
        for (size_t k = 0; k < n; k += 2*m)
        {
            FieldT w = FieldT::one();
            for (size_t j = 0; j < m; ++j)
            {
                const FieldT t = w * a[k+j+m];
                a[k+j+m] = a[k+j] - t;
                a[k+j] += t;
                w *= w_m;
            }
        }
#ifndef _MSC_VER
        asm volatile ("/* post-inner */");
#endif
        m *= 2;
    }
}

template<typename FieldT>
void _basic_parallel_radix2_FFT_inner(std::vector<FieldT> &a, const FieldT &omega, const size_t log_cpus)
{
    const size_t num_cpus = ((size_t)1) <<log_cpus;

    const size_t m = a.size();
    const size_t log_m = libff::log2(m);
    if (m != ((size_t)1) <<log_m) throw DomainSizeException("expected m == ((size_t)1)<<log_m");

    if (log_m < log_cpus)
    {
        _basic_serial_radix2_FFT(a, omega);
        return;
    }

    std::vector<std::vector<FieldT> > tmp(num_cpus);
    for (size_t j = 0; j < num_cpus; ++j)
    {
        tmp[j].resize(((size_t)1) <<(log_m-log_cpus), FieldT::zero());
    }

#ifdef MULTICORE
    #pragma omp parallel for
#endif
    for (size_t j = 0; j < num_cpus; ++j)
    {
        const FieldT omega_j = omega^j;
        const FieldT omega_step = omega^(j<<(log_m - log_cpus));

        FieldT elt = FieldT::one();
        for (size_t i = 0; i < ((size_t)1) <<(log_m - log_cpus); ++i)
        {
            for (size_t s = 0; s < num_cpus; ++s)
            {
                // invariant: elt is omega^(j*idx)
                const size_t idx = (i + (s<<(log_m - log_cpus))) % (((size_t)1) << log_m);
                tmp[j][i] += a[idx] * elt;
                elt *= omega_step;
            }
            elt *= omega_j;
        }
    }

    const FieldT omega_num_cpus = omega^num_cpus;

#ifdef MULTICORE
    #pragma omp parallel for
#endif
    for (size_t j = 0; j < num_cpus; ++j)
    {
        _basic_serial_radix2_FFT(tmp[j], omega_num_cpus);
    }

#ifdef MULTICORE
    #pragma omp parallel for
#endif
    for (size_t i = 0; i < num_cpus; ++i)
    {
        for (size_t j = 0; j < ((size_t)1) <<(log_m - log_cpus); ++j)
        {
            // now: i = idx >> (log_m - log_cpus) and j = idx % (1u << (log_m - log_cpus)), for idx = ((i<<(log_m-log_cpus))+j) % (1u << log_m)
            a[(j<<log_cpus) + i] = tmp[i][j];
        }
    }
}

template<typename FieldT>
void _basic_parallel_radix2_FFT(std::vector<FieldT> &a, const FieldT &omega)
{
#ifdef MULTICORE
    const size_t num_cpus = omp_get_max_threads();
#else
    const size_t num_cpus = 1;
#endif
    const size_t log_cpus = ((num_cpus & (num_cpus - 1)) == 0 ? libff::log2(num_cpus) : libff::log2(num_cpus) - 1);

#ifdef DEBUG
    libff::print_indent(); printf("* Invoking parallel FFT on 2^%zu CPUs (omp_get_max_threads = %zu)\n", log_cpus, num_cpus);
#endif

    if (log_cpus == 0)
    {
        _basic_serial_radix2_FFT(a, omega);
    }
    else
    {
        _basic_parallel_radix2_FFT_inner(a, omega, log_cpus);
    }
}

template<typename FieldT>
void _multiply_by_coset(std::vector<FieldT> &a, const FieldT &g)
{
    FieldT u = g;
    for (size_t i = 1; i < a.size(); ++i)
    {
        a[i] *= u;
        u *= g;
    }
}

template<typename FieldT>
std::vector<FieldT> _basic_radix2_evaluate_all_lagrange_polynomials(const size_t m, const FieldT &t)
{
    if (m == 1)
    {
        return std::vector<FieldT>(1, FieldT::one());
    }

    if (m != ((size_t)1 << libff::log2(m))) throw DomainSizeException("expected m == (1u << libff::log2(m))");

    const FieldT omega = libff::get_root_of_unity<FieldT>(m);

    std::vector<FieldT> u(m, FieldT::zero());

    /*
     If t equals one of the roots of unity in S={omega^{0},...,omega^{m-1}}
     then output 1 at the right place, and 0 elsewhere
     */

    if ((t^m) == (FieldT::one()))
    {
        FieldT omega_i = FieldT::one();
        for (size_t i = 0; i < m; ++i)
        {
            if (omega_i == t) // i.e., t equals omega^i
            {
                u[i] = FieldT::one();
                return u;
            }

            omega_i *= omega;
        }
    }

    /*
     Otherwise, if t does not equal any of the roots of unity in S,
     then compute each L_{i,S}(t) as Z_{S}(t) * v_i / (t-\omega^i)
     where:
     - Z_{S}(t) = \prod_{j} (t-\omega^j) = (t^m-1), and
     - v_{i} = 1 / \prod_{j \neq i} (\omega^i-\omega^j).
     Below we use the fact that v_{0} = 1/m and v_{i+1} = \omega * v_{i}.
     */

    const FieldT Z = (t^m)-FieldT::one();
    FieldT l = Z * FieldT(m).inverse();
    FieldT r = FieldT::one();
    for (size_t i = 0; i < m; ++i)
    {
        u[i] = l * (t - r).inverse();
        l *= omega;
        r *= omega;
    }

    return u;
}

} // libfqfft

#endif // BASIC_RADIX2_DOMAIN_AUX_TCC_
