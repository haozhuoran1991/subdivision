// doosabin.h
#ifndef DOOSABIN_H
#define DOOSABIN_H

// Includes
#include <algorithm>
#include <memory>
#include <vector>

#include "Eigen/Dense"

// TODO Move these to common.
#include "Mesh/face_array.h"
#include "Math/modulo.h"

#include "uniform_quadratic_bspline.h"

// doosabin
namespace doosabin {

using face_array::FaceArray;

using modulo::modulo;

// DooSabinWeights
template <typename Weights>
void DooSabinWeights(int N, Weights* w) {
  typedef typename Weights::Scalar T;

  assert(N >= 3);

  if (w->size() != N) {
    w->resize(N);
  }

  for (int i = 1; i < N; ++i) {
    (*w)[i] = (3 + 2 * cos(2 * T(M_PI) * i / N)) / (4 * N);
  }
  (*w)[0] = T(N + 5) / (4 * N);
}

// kNumBiquadraticBsplineBasis
const size_t kNumBiquadraticBsplineBasis = 9;

// kBiquadraticBsplineBasis
const int kBiquadraticBsplineBasis[kNumBiquadraticBsplineBasis][2] = {{1, 1},
                                                                      {1, 0},
                                                                      {0, 0},
                                                                      {0, 1},
                                                                      {0, 2},
                                                                      {1, 2},
                                                                      {2, 2},
                                                                      {2, 1},
                                                                      {2, 0}};

// BiquadraticBsplineBasis
template <typename F, typename G, typename U, typename B>
inline void BiquadraticBsplineBasis(const U& u, B* b) {
  static const F f;
  static const G g;
  for (typename B::Index i = 0; i < b->size(); ++i) {
    (*b)[i] = f(u[0], kBiquadraticBsplineBasis[i][0]) *
              g(u[1], kBiquadraticBsplineBasis[i][1]);
  }
}

// kValidUOffsets
static const int kValidUOffsets[][2] = {{-1, -1},
                                        {-1,  1},
                                        { 1,  1},
                                        { 1, -1}};

// kMaxSubdivisionDepth
static const size_t kMaxSubdivisionDepth = 10;

// kValidUEpsilon
static const double kValidUEpsilon = 1e-6;

// Patch (forward declaration)
template <typename Scalar>
class Patch;

// InternalPatch
template <typename Scalar>
class InternalPatch
{
public:
  typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> Matrix;
  typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1> Vector;

  InternalPatch(const InternalPatch * parent, size_t depth,
                FaceArray && face_array)
    : _parent(parent), _depth(depth),
      _face_array(std::move(face_array)),
      _root(nullptr),
      _V_is_valid(false)
  {
    OrderPatch();
    SetIsPatchValid();
    SetPatchVertexIndices();
  }

  // EvaluatePosition
  template <typename Tu, typename Tr>
  void EvaluatePosition(Tu & u, Tr & r)
  {
    return Evaluate<uniform_quadratic_bspline::Position,
                    uniform_quadratic_bspline::Position,
                    EvaluatePositionFunctor>(u, r);
  }

  const std::vector<int> & PatchIndices() const
  {
    return _I;
  }

  int GetAdjacentVertexOffset(int adjacent_vertex) const
  {
    for (size_t i = 0; i < _face_array.GetNumberOfFaces(); ++i)
    {
      auto face = _face_array.GetFace(i);
      if (face[1] == adjacent_vertex)
        return static_cast<int>(i);
    }

    return -1;
  }

  int GetFaceOffset(const std::vector<int> & face)
  {
    auto it = std::find(face.begin(), face.end(), _i);
    if (it == face.end())
      return -1;

    size_t j = std::distance(face.begin(), it);
    int next_vertex = face[(j + 1) % face.size()];

    return _face_array.FindHalfEdge(_i, next_vertex);
  }

  bool IsValid() const { return _is_valid; }

// FIXME Was `protected`.
public:
  // Initialisation
  void OrderPatch()
  {
    // determine the common vertex for all faces and store in `_i`
    _i = _face_array.FindCommonVertex();
    assert(_i >= 0);

    // order the vertices in each face so that `_i` is the first vertex
    // order the faces to also respect the vertex ordering
    const size_t n_faces = _face_array.GetNumberOfFaces();
    size_t faces_remaining = n_faces;

    std::vector<size_t> ordered_face_indices;
    ordered_face_indices.reserve(n_faces);

    // order first face
    _face_array.RotateFaceToVertex(0, _i);
    ordered_face_indices.push_back(0);

    while (--faces_remaining)
    {
      size_t last_face_index = ordered_face_indices.back();
      int n = _face_array.GetNumberOfSides(last_face_index);
      auto p = _face_array.GetFace(last_face_index);
      const int last_vertex = p[n - 1];

      // search for half-edge (`i`, `last_vertex`) to find `next_face_offset`
      int next_face_index = -1;

      for (size_t j = 0; j < n_faces; ++j)
      {
        n = _face_array.GetNumberOfSides(j);
        p = _face_array.GetFace(j);

        for (int k = 0; k < n; ++k)
        {
          if (_i == p[k] && last_vertex == p[(k + 1) % n])
          {
            next_face_index = j;
            break;
          }
        }

        if (next_face_index >= 0)
          break;
      }

      // unable to find half-edge
      assert(next_face_index >= 0);

      _face_array.RotateFaceToVertex(next_face_index, _i);
      ordered_face_indices.push_back(next_face_index);
    }

    _face_array.PermuteFaces(ordered_face_indices);
  }

  void SetIsPatchValid()
  {
    const size_t n_faces = _face_array.GetNumberOfFaces();
    for (size_t j = 0; j < n_faces; ++j)
    {
      if (_face_array.GetNumberOfSides(j) != 4)
      {
        _is_valid = false;
        return;
      }
    }

    _is_valid = true;
  }

  void SetPatchVertexIndices()
  {
    // initialise `_I`
    _I.push_back(_i);

    const size_t n_faces = _face_array.GetNumberOfFaces();
    for (size_t j = 0; j < n_faces; ++j)
    {
      const int n = _face_array.GetNumberOfSides(j);
      auto p = _face_array.GetFace(j);
      std::copy(p + 1, p + n - 1, back_inserter(_I));
    }

    // initialise `_S`
    const int n = _I.size();
    _S.setIdentity(n, n);
  }

  // Subdivision (core)
  void SubdivideChildren()
  {
    // don't subdivide twice
    if (_children.size() > 0)
      return;

    const size_t n_faces = _face_array.GetNumberOfFaces();
    assert(n_faces == 4);

    // create `S` to include the all child vertices
    size_t n_child_vertices = 0;
    for (size_t i = 0; i < n_faces; ++i)
      n_child_vertices += _face_array.GetNumberOfSides(i);

    Matrix S(n_child_vertices, _I.size());
    S.fill(0.0);

    // fill `S` using the Doo-Sabin subdivision weights
    size_t child_index = 0;
    for (size_t i = 0; i < n_faces; ++i)
    {
      // get subdivision weights of face `i` with `n` vertices
      const int n = _face_array.GetNumberOfSides(i);
      Vector w;
      DooSabinWeights(n, &w);

      // get `face_indices_in_I`
      std::vector<int> face_indices_in_I(n);
      auto p = _face_array.GetFace(i);

      for (int j = 0; j < n; ++j)
      {
        // find index of vertex `p[j]` in `_I`
        auto it = std::find(_I.begin(), _I.end(), p[j]);
        assert(it != _I.end());
        face_indices_in_I[j] = std::distance(_I.begin(), it);
      }

      // copy `w` into `S` for each child vertex
      for (int j = 0; j < n; ++j)
      {
        for (int k = 0; k < n; ++k)
          S(child_index, face_indices_in_I[k]) = w[modulo(k - j, n)];

        ++child_index;
      }
    }

    // get `S` to reference the top-level vertices
    assert(_S.rows() > 0 && _S.cols() > 0);
    S *= _S;

    // build `child_face_array`
    child_index = 0;
    std::vector<int> raw_child_face_array;
    raw_child_face_array.push_back(n_faces);
    for (size_t i = 0; i < n_faces; ++i)
    {
      int n = _face_array.GetNumberOfSides(i);
      raw_child_face_array.push_back(n);

      for (int j = 0; j < n; ++j)
        raw_child_face_array.push_back(child_index++);
    }
    FaceArray child_face_array(std::move(raw_child_face_array));

    // build child patches
    for (size_t i = 0; i < n_faces; ++i)
    {
      // four child faces are created because patch is valency 4
      raw_child_face_array.push_back(4);

      auto face = child_face_array.GetFace(i);
      auto next_face = child_face_array.GetFace(modulo(i + 1, n_faces));
      auto opp_face = child_face_array.GetFace(modulo(i + 2, n_faces));
      auto prev_face = child_face_array.GetFace(modulo(i + 3, n_faces));

      // first child face
      const int n = child_face_array.GetNumberOfSides(i);

      raw_child_face_array.push_back(n);
      std::copy(face, face + n, back_inserter(raw_child_face_array));

      // next three generated faces
      const int n_prev = child_face_array.GetNumberOfSides((i + 3) % n_faces);

      const int child_faces[][4] = {
        {face[0], face[modulo(-1, n)], next_face[1], next_face[0]},
        {face[0], next_face[0], opp_face[0], prev_face[0]},
        {face[0], prev_face[0], prev_face[modulo(-1, n_prev)], face[1]}
      };

      for (int j = 0; j < 3; ++j)
      {
        raw_child_face_array.push_back(4);
        std::copy(child_faces[j], child_faces[j] + 4, back_inserter(raw_child_face_array));
      }

      // build `child_patch_array`
      FaceArray child_patch_array(std::move(raw_child_face_array));

      // permute the patch faces to preserve `u` directionality
      std::vector<size_t> child_face_permutation(4);
      child_face_permutation[0] = static_cast<size_t>(modulo(0 - i, 4));
      child_face_permutation[1] = static_cast<size_t>(modulo(1 - i, 4));
      child_face_permutation[2] = static_cast<size_t>(modulo(2 - i, 4));
      child_face_permutation[3] = static_cast<size_t>(modulo(3 - i, 4));
      child_patch_array.PermuteFaces(child_face_permutation);

      // build `child`
      std::unique_ptr<InternalPatch<Scalar> > child(new InternalPatch<Scalar>(
        this, _depth + 1, std::move(child_patch_array)));
      // propagate root
      child->_root = _root;

      // build `child._S` and move to `_children`
      child->_S.resize(child->_I.size(), S.cols());
      for (size_t i = 0; i < child->_I.size(); ++i)
        child->_S.row(i) = S.row(child->_I[i]);

      _children.push_back(std::move(child));
    }
  }

  // Evaluate
  template <typename FB0,
            typename FB1,
            typename FEval,
            typename Tu,
            typename Tr>
  void Evaluate(Tu & u, Tr & r)
  {
    if (_is_valid)
    {
      // get the basis vector for the quantity
      Eigen::Matrix<Scalar, kNumBiquadraticBsplineBasis, 1> b;
      BiquadraticBsplineBasis<FB0, FB1>(u, &b);

      static const FEval feval;
      feval(*this, b, r);

      return;
    }

    SubdivideChildren();

    if (_depth == (kMaxSubdivisionDepth - 1))
    {
      // on second to last level of subdivision, adjust `U` so that it
      // falls within a valid patch
      AdjustUForValidChild(u);
    }

    // get child and translate `u` for child
    int child_index = PassToChild(u);
    return _children[child_index]->Evaluate<FB0, FB1, FEval>(u, r);
  }

  template <typename Tu>
  void AdjustUForValidChild(Tu & u) const
  {
    assert(_children.size() > 0);

    for (size_t i = 0; i < _children.size(); ++i)
    {
      InternalPatch<Scalar> & child = *_children[i];
      if (child._is_valid)
      {
        u[0] = 0.5 + kValidUOffsets[i][0] * Scalar(kValidUEpsilon);
        u[1] = 0.5 + kValidUOffsets[i][1] * Scalar(kValidUEpsilon);
        return;
      }
    }
  }

  template <typename Tu>
  int PassToChild(Tu & u) const
  {
    int child_index = -1;

    if (u[0] >= 0.5)
    {
      u[0] -= 0.5;

      if (u[1] >= 0.5)
      {
        u[1] -= 0.5;
        child_index = 2;
      }
      else
      {
        child_index = 3;
      }
    }
    else
    {
      if (u[1] >= 0.5)
      {
        u[1] -= 0.5;
        child_index = 1;
      }
      else
      {
        child_index = 0;
      }
    }

    u[0] *= 2.0;
    u[1] *= 2.0;

    return child_index;
  }

  void InvalidateVertices()
  {
    _V_is_valid = false;

    for (auto & child : _children)
      child->InvalidateVertices();
  }

  const Matrix & V()
  {
    if (!_V_is_valid)
    {
      _V.noalias() = _root->Vertices() * _S.transpose();
      _V_is_valid = true;
    }

    return _V;
  }

  // EvaluatePosition
  class EvaluatePositionFunctor
  {
  public:
    template <typename Tb, typename Tr>
    void operator()(InternalPatch<Scalar> & patch, const Tb & b, Tr & r) const
    {
      r.noalias() = patch.V() * b;
    }
  };

  // Evaluation
protected:
  const InternalPatch<Scalar> * _parent;
  size_t _depth;
  FaceArray _face_array;

  const Patch<Scalar> * _root;

  bool _V_is_valid;

  int _i;
  std::vector<int> _I;
  bool _is_valid;

  std::vector<std::unique_ptr<InternalPatch<Scalar>>> _children;
  Matrix _S;
  Matrix _V;
};

// Patch
template <typename Scalar>
class Patch : public InternalPatch<Scalar>
{
public:
  Patch(FaceArray && face_array)
    : InternalPatch<Scalar>(nullptr, 0, std::move(face_array))
  {
    this->_root = this;
  }

  // SetVertices
  template <typename TV>
  void SetVertices(const TV & V)
  {
    _V0 = V;
    this->InvalidateVertices();
  }

  // Vertices
  const Matrix & Vertices() const
  {
    return _V0;
  }

protected:
  Matrix _V0;
};



} // namespace doosabin

#endif // DOOSABIN_H