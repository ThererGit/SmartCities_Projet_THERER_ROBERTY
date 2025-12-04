from sqlalchemy.orm import DeclarativeBase, mapped_column, relationship
from sqlalchemy import Integer, String, Date, ForeignKey, create_engine
from datetime import date


# --- Base commune ---
class Base(DeclarativeBase):
    pass


# --- Table Genre ---
class Genre(Base):
    __tablename__ = "Genre"
    id = mapped_column(Integer, primary_key=True)
    genre = mapped_column(String(50), nullable=False, unique=True)

    # relation avec les films
    films = relationship("Film", back_populates="genre_obj")

    def __repr__(self):
        return f"<Genre(id={self.id}, genre='{self.genre}')>"


# --- Table Film ---
class Film(Base):
    __tablename__ = "Film"

    id = mapped_column(Integer, primary_key=True)
    titre = mapped_column(String(100), nullable=False)
    note = mapped_column(Integer)  # note sur 5
    date_sortie = mapped_column(Date)
    duree = mapped_column(Integer)  # durée en minutes
    genre_id = mapped_column(Integer, ForeignKey("Genre.id"))

    # relation avec la table Genre
    genre_obj = relationship("Genre", back_populates="films")

    def __repr__(self):
        return (
            f"<Film(id={self.id}, titre='{self.titre}', note={self.note}, "
            f"date_sortie={self.date_sortie}, duree={self.duree}, genre_id={self.genre_id})>"
        )


# --- Connexion à MariaDB ---
engine = create_engine(
    "mariadb+mariadbconnector://mat:123456789@172.20.10.3:3306/RPG", echo=True
)

# --- Création des tables dans la base ---
Base.metadata.create_all(engine)
