
#ifndef COMPLIANT_ASSEMBLEDMAPPING_H
#define COMPLIANT_ASSEMBLEDMAPPING_H

#include <sofa/core/MechanicalParams.h>
#include <sofa/core/Mapping.h>
#include <SofaEigen2Solver/EigenSparseMatrix.h>
#include <Compliant/config.h>

namespace sofa {
	namespace component {
		namespace mapping {

			// assembled mapping base class: derived classes only need to
			// implement apply and assemble
			template<class In, class Out>
			class AssembledMapping : public core::Mapping<In, Out> {

				typedef AssembledMapping self;
                typedef typename core::Mapping<In, Out> base;
	
                typedef helper::vector<sofa::defaulttype::BaseMatrix*> js_type;
				js_type js;


            protected:

                AssembledMapping()
                {
                    js.resize(1);
                    js[0] = &jacobian;
                }



			public:

				SOFA_ABSTRACT_CLASS(SOFA_TEMPLATE2(AssembledMapping,In,Out), SOFA_TEMPLATE2(core::Mapping,In,Out));

                typedef typename Out::Real Real; // used in Mapping_test

                void update()
                {
                    this->reinit();
                    base::apply(core::mechanicalparams::defaultInstance(), core::VecCoordId::position(), core::ConstVecCoordId::position());
                    base::applyJ(core::mechanicalparams::defaultInstance(), core::VecDerivId::velocity(), core::ConstVecDerivId::velocity());
                    if (this->f_applyRestPosition.getValue())
                        base::apply(core::mechanicalparams::defaultInstance(), core::VecCoordId::restPosition(), core::ConstVecCoordId::restPosition());
                }
	
                const helper::vector<sofa::defaulttype::BaseMatrix*>* getJs() override {
					assert( !js.empty() );
					return &js;
				}

                const sofa::defaulttype::BaseMatrix* getJ() override { return &jacobian; }
	
				virtual void apply(const core::MechanicalParams*,
				                   Data<typename self::OutVecCoord>& out, 
                                   const Data<typename self::InVecCoord>& in) override {
					out_pos_type out_pos(out);
					in_pos_type in_pos(in);
	  
					apply(out_pos, in_pos);
					assemble( in_pos );
				}
	
				virtual void applyJ(const core::MechanicalParams*,
				                    Data<typename self::OutVecDeriv>& out, 
                                    const Data<typename self::InVecDeriv>& in) override {
					if( jacobian.compressedMatrix.nonZeros() > 0 ) {
                        jacobian.mult(out, in);
                    }
				}

				void debug() {
					std::cerr << this->getClassName() << std::endl;
					std::cerr << "from: " <<  this->getFromModel()->getContext()->getName() 
							  << "/" << this->getFromModel()->getName() << std::endl;
					std::cerr << "to: " <<  this->getToModel()->getContext()->getName() 
							  << "/" << this->getToModel()->getName() << std::endl;
					std::cerr << std::endl;
				}

				virtual void applyJT(const core::MechanicalParams*,			     
				                     Data<typename self::InVecDeriv>& in, 
                                     const Data<typename self::OutVecDeriv>& out) override {
					if( jacobian.compressedMatrix.nonZeros() > 0 ) {
                        jacobian.addMultTranspose(in, out);
                    }
				}

				virtual void applyJT(const core::ConstraintParams*,
				                     Data< typename self::InMatrixDeriv>& , 
                                     const Data<typename self::OutMatrixDeriv>& ) override {
				}


                virtual void updateK( const core::MechanicalParams* /*mparams*/, core::ConstMultiVecDerivId childForce ) override {

                    // trigger assembly
                    this->assemble_geometric(this->in_pos(),
                                             this->out_force( childForce ) );
                }

                virtual const defaulttype::BaseMatrix* getK() override {

                    if( geometric.compressedMatrix.nonZeros() ) return &geometric;
                    else return NULL;
                }

                virtual void applyDJT(const core::MechanicalParams* mparams,
                                      core::MultiVecDerivId inForce,
                                      core::ConstMultiVecDerivId /* inDx */ ) override {

                    if( geometric.compressedMatrix.nonZeros() ) {

                        const Data<typename self::InVecDeriv>& inDx =
                            *mparams->readDx(this->fromModel);
                        
                        core::State<In>* from_write = this->getFromModel();

                        // TODO does this even make sense ?
                        geometric.addMult(*inForce[from_write].write(),
                                          inDx,
                                          sofa::core::mechanicalparams::kFactor(mparams));
                    }

                    
                }
                

			protected:
				enum {Nin = In::deriv_total_size,
                      Nout = Out::deriv_total_size };

				typedef helper::ReadAccessor< Data< typename self::InVecCoord > > in_pos_type;
                
                typedef helper::WriteOnlyAccessor< Data< typename self::OutVecCoord > > out_pos_type;

                typedef helper::ReadAccessor< Data< typename self::OutVecDeriv > > out_force_type;

                typedef helper::WriteAccessor< Data< typename self::InVecDeriv > > in_vel_type;
                
				in_pos_type in_pos() {

					const core::State<In>* fromModel = this->getFromModel();
					assert( fromModel );
					
					core::ConstMultiVecCoordId inPos = core::ConstVecCoordId::position();
	  
					const typename self::InDataVecCoord* in = inPos[fromModel].read();
					
					return *in;
				}


                out_force_type out_force( core::ConstMultiVecDerivId outForce ) {

					const core::State<Out>* toModel = this->getToModel();
                    assert( toModel );
	  
					const typename self::OutDataVecDeriv* out = outForce[toModel].read();
					
					return *out;
				}



				virtual void assemble( const in_pos_type& in ) = 0;
                virtual void assemble_geometric( const in_pos_type& /*in*/,
                                                 const out_force_type& /*out*/) { }
                
                using core::Mapping<In, Out>::apply;
				virtual void apply(out_pos_type& out, const in_pos_type& in ) = 0;
	
				typedef linearsolver::EigenSparseMatrix<In, Out> jacobian_type;
				jacobian_type jacobian;

                typedef linearsolver::EigenSparseMatrix<In, In> geometric_type;
                geometric_type geometric;

			};

		}
	}
}



#endif
